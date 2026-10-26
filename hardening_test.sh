#!/usr/bin/env bash
set -eu
tmpdir="$(mktemp -d)"
cleanup() { kill "${api_pid:-}" "${gateway_pid:-}" "${alias_gateway_pid:-}" "${backend_pid:-}" 2>/dev/null || true; rm -rf "$tmpdir"; }
trap cleanup EXIT
backend_port=14911; gateway_port=14910; api_port=18081
./backend-emulator "$backend_port" 2>"$tmpdir/backend.log" & backend_pid=$!
./ironbridge "$gateway_port" 127.0.0.1 "$backend_port" copybook.cpy 2>"$tmpdir/gateway.log" & gateway_pid=$!
./ironbridge 14912 127.0.0.1 "$backend_port" copybook-alias.cpy copybook-alias.map 2>"$tmpdir/alias-gateway.log" & alias_gateway_pid=$!
python3 api.py --port "$api_port" --gateway-port "$gateway_port" 2>"$tmpdir/api.log" & api_pid=$!
for _ in $(seq 1 30); do
  python3 -c 'import socket; s=socket.create_connection(("127.0.0.1", 14910), .1); s.close()' 2>/dev/null && break || sleep .1
done

python3 - <<'PY'
import socket, time
payload = b'{"transaction_id":"FRAG000000000001","account_id":"123456789012","amount":"1.00","currency":"BRL","operation":"DEBIT"}\n'
with socket.create_connection(("127.0.0.1", 14910), timeout=3) as s:
    for byte in payload:
        s.sendall(bytes([byte]))
        time.sleep(0.001)
    response = s.recv(2048).decode()
assert '"status":"OK"' in response, response
PY

python3 - <<'PY'
import json, socket
request = json.dumps({"transaction_id":"ALIAS00000000001","account_id":"123456789012","amount":"1.00","currency":"BRL","operation":"DEBIT"}) + "\n"
with socket.create_connection(("127.0.0.1", 14912), timeout=3) as s:
    s.sendall(request.encode())
    response = s.recv(2048).decode()
assert '"status":"OK"' in response, response
PY

nan_response="$(python3 - <<'PY'
import socket
with socket.create_connection(("127.0.0.1", 14910), timeout=3) as s:
    s.sendall(b'{"transaction_id":"NAN000000000001","account_id":"123456789012","amount":"NaN","currency":"BRL","operation":"DEBIT"}\n')
    print(s.recv(2048).decode())
PY
)"
echo "$nan_response" | grep -q '"code":"400"'

ready="$(curl -sS -o /dev/null -w '%{http_code}' "http://127.0.0.1:$api_port/ready")"
test "$ready" = 200
body="$(python3 -c 'print("x" * 9000)')"
large="$(curl -sS -o /dev/null -w '%{http_code}' -X POST "http://127.0.0.1:$api_port/transactions" -H 'Content-Type: application/json' --data-binary "$body")"
test "$large" = 400
kill "$gateway_pid"; wait "$gateway_pid" 2>/dev/null || true; unset gateway_pid
not_ready="$(curl -sS -o /dev/null -w '%{http_code}' "http://127.0.0.1:$api_port/ready")"
test "$not_ready" = 503
echo "testes de robustez TCP/HTTP: OK"

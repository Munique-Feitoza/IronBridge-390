#!/usr/bin/env bash
set -eu
tmpdir="$(mktemp -d)"
cleanup() { kill "${backend_pid:-}" "${bridge_pid:-}" 2>/dev/null || true; rm -rf "$tmpdir"; }
trap cleanup EXIT
./backend-emulator 13901 2>"$tmpdir/backend.log" & backend_pid=$!
./ironbridge 13900 127.0.0.1 13901 2>"$tmpdir/bridge.log" & bridge_pid=$!
for _ in $(seq 1 20); do python3 -c 'import socket; s=socket.socket(); s.settimeout(.1); s.connect(("127.0.0.1",13900)); s.close()' 2>/dev/null && break || sleep .05; done
ok="$(python3 client.py --port 13900 --transaction-id TRX000000000001 --account-id 123456789012 --amount 125.37 --currency BRL --operation DEBIT)"
echo "$ok" | grep -q "'status': 'OK'"
invalid="$(python3 client.py --port 13900 --transaction-id TRX000000000002 --account-id 123456789012 --amount 0.00 --currency BRL --operation DEBIT)"
echo "$invalid" | grep -q "'code': '422'"
currency="$(python3 client.py --port 13900 --transaction-id TRX000000000003 --account-id 123456789012 --amount 10.00 --currency USD --operation DEBIT)"
echo "$currency" | grep -q "'code': '422'"
operation="$(python3 client.py --port 13900 --transaction-id TRX000000000004 --account-id 123456789012 --amount 10.00 --currency BRL --operation XXXXX)"
echo "$operation" | grep -q "'code': '422'"
credit="$(python3 client.py --port 13900 --transaction-id TRX000000000006 --account-id 123456789012 --amount 10.00 --currency BRL --operation CRED)"
echo "$credit" | grep -q "'status': 'OK'"
malformed="$(python3 - <<'PY'
import socket
s = socket.create_connection(('127.0.0.1', 13900)); s.sendall(b'{bad-json}\n'); print(s.recv(2048).decode())
PY
)"
echo "$malformed" | grep -q '"code":"400"'
for i in $(seq 1 20); do
  python3 client.py --port 13900 --transaction-id "TRX$(printf '%012d' "$i")" --account-id 123456789012 --amount 1.00 --currency BRL --operation DEBIT >/dev/null
done
printf '       01 WRONG-RECORD.\n          05 VALUE PIC X(1).\n' >"$tmpdir/bad.cpy"
if ./ironbridge 13902 127.0.0.1 13999 "$tmpdir/bad.cpy" >/dev/null 2>&1; then exit 1; fi
kill "$backend_pid" 2>/dev/null || true; wait "$backend_pid" 2>/dev/null || true
unavailable="$(python3 client.py --port 13900 --transaction-id TRX000000000005 --account-id 123456789012 --amount 1.00 --currency BRL --operation DEBIT)"
echo "$unavailable" | grep -q "'code': '503'"
echo "testes end-to-end: OK"

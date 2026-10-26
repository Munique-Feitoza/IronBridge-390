#!/usr/bin/env bash
set -eu
tmpdir="$(mktemp -d)"
cleanup() { kill "${api_pid:-}" "${gateway_pid:-}" "${backend_pid:-}" 2>/dev/null || true; rm -rf "$tmpdir"; }
trap cleanup EXIT
pick_port() { python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()'; }
backend_port="$(pick_port)"; gateway_port="$(pick_port)"; api_port="$(pick_port)"
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost -keyout "$tmpdir/key.pem" -out "$tmpdir/cert.pem" >/dev/null 2>&1
./backend-emulator "$backend_port" 2>"$tmpdir/backend.log" & backend_pid=$!
./ironbridge "$gateway_port" 127.0.0.1 "$backend_port" copybook.cpy 2>"$tmpdir/gateway.log" & gateway_pid=$!
python3 api.py --port "$api_port" --gateway-port "$gateway_port" --token dev-secret --tls-cert "$tmpdir/cert.pem" --tls-key "$tmpdir/key.pem" 2>"$tmpdir/api.log" & api_pid=$!
sleep .3
unauthorized="$(curl -ksS -o "$tmpdir/unauthorized.json" -w '%{http_code}' -X POST "https://127.0.0.1:$api_port/transactions" -H 'Content-Type: application/json' -d '{}')"
test "$unauthorized" = 401
authorized="$(curl -ksSf -X POST "https://127.0.0.1:$api_port/transactions" -H 'Authorization: Bearer dev-secret' -H 'Content-Type: application/json' -d '{"transaction_id":"SEC000000000001","account_id":"123456789012","amount":"1.00","currency":"BRL","operation":"DEBIT"}')"
echo "$authorized" | grep -q '"status": "OK"'
echo "teste Bearer token + TLS: OK"

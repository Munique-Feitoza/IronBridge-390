#!/usr/bin/env bash
set -eu
tmpdir="$(mktemp -d)"
cleanup() { kill "${api_pid:-}" "${gateway_pid:-}" "${cobol_pid:-}" 2>/dev/null || true; rm -rf "$tmpdir"; }
trap cleanup EXIT
pick_port() { python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()'; }
cobol_port="$(pick_port)"
gateway_port="$(pick_port)"
api_port="$(pick_port)"
./cobol-backend "$cobol_port" 2>"$tmpdir/cobol.log" & cobol_pid=$!
sleep .1
if ! kill -0 "$cobol_pid" 2>/dev/null; then cat "$tmpdir/cobol.log"; exit 1; fi
./ironbridge "$gateway_port" 127.0.0.1 "$cobol_port" copybook.cpy 2>"$tmpdir/gateway.log" & gateway_pid=$!
python3 api.py --port "$api_port" --gateway-port "$gateway_port" 2>"$tmpdir/api.log" & api_pid=$!
for _ in $(seq 1 30); do
  python3 -c "import socket; s=socket.create_connection(('127.0.0.1',$api_port), .1); s.close()" 2>/dev/null && break || sleep .1
done
valid="$(curl -fsS -X POST "http://127.0.0.1:$api_port/transactions" -H 'Content-Type: application/json' -d '{"transaction_id":"TRX000000000001","account_id":"123456789012","amount":"125.37","currency":"BRL","operation":"DEBIT"}')"
echo "$valid" | grep -q 'Transacao validada pelo core COBOL'
invalid="$(curl -sS -X POST "http://127.0.0.1:$api_port/transactions" -H 'Content-Type: application/json' -d '{"transaction_id":"TRX000000000002","account_id":"123456789012","amount":"0.00","currency":"BRL","operation":"DEBIT"}')"
echo "$invalid" | grep -q '"code": "422"'
echo "teste REST HTTP -> gateway C -> backend COBOL: OK"

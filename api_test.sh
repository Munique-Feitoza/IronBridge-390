#!/usr/bin/env bash
set -eu
tmpdir="$(mktemp -d)"
cleanup() { kill "${api_pid:-}" "${gateway_pid:-}" "${backend_pid:-}" 2>/dev/null || true; rm -rf "$tmpdir"; }
trap cleanup EXIT
./backend-emulator 13911 2>"$tmpdir/backend.log" & backend_pid=$!
./ironbridge 13910 127.0.0.1 13911 copybook.cpy 2>"$tmpdir/gateway.log" & gateway_pid=$!
python3 api.py --port 18080 --gateway-port 13910 --db "$tmpdir/idempotency.db" 2>"$tmpdir/api.log" & api_pid=$!
for _ in $(seq 1 30); do
  python3 -c 'import urllib.request; urllib.request.urlopen("http://127.0.0.1:18080/", timeout=.1)' 2>/dev/null && break || sleep .1
done
valid="$(curl -fsS -X POST http://127.0.0.1:18080/transactions -H 'Content-Type: application/json' -d '{"transaction_id":"TRX000000000001","account_id":"123456789012","amount":"125.37","currency":"BRL","operation":"DEBIT"}')"
echo "$valid" | grep -q '"status": "OK"'
invalid="$(curl -sS -X POST http://127.0.0.1:18080/transactions -H 'Content-Type: application/json' -d '{"transaction_id":"TRX000000000002","account_id":"123456789012","amount":"0.00","currency":"BRL","operation":"DEBIT"}')"
echo "$invalid" | grep -q '"code": "422"'
first="$(curl -fsS -X POST http://127.0.0.1:18080/transactions -H 'Content-Type: application/json' -H 'Idempotency-Key: payment-001' -d '{"transaction_id":"TRX000000000003","account_id":"123456789012","amount":"10.00","currency":"BRL","operation":"DEBIT"}')"
second="$(curl -fsS -X POST http://127.0.0.1:18080/transactions -H 'Content-Type: application/json' -H 'Idempotency-Key: payment-001' -d '{"transaction_id":"TRX000000000003","account_id":"123456789012","amount":"10.00","currency":"BRL","operation":"DEBIT"}')"
test "$first" = "$second"
conflict="$(curl -sS -o /dev/null -w '%{http_code}' -X POST http://127.0.0.1:18080/transactions -H 'Content-Type: application/json' -H 'Idempotency-Key: payment-001' -d '{"transaction_id":"TRX000000000004","account_id":"123456789012","amount":"11.00","currency":"BRL","operation":"DEBIT"}')"
test "$conflict" = 409
echo "teste REST HTTP: OK"

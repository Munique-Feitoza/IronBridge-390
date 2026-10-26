#!/usr/bin/env python3
import argparse
import json
import socket

p = argparse.ArgumentParser(description="Cliente JSON do IronBridge-390")
p.add_argument("--host", default="127.0.0.1")
p.add_argument("--port", type=int, default=3900)
p.add_argument("--transaction-id", required=True)
p.add_argument("--account-id", required=True)
p.add_argument("--amount", required=True)
p.add_argument("--currency", default="BRL")
p.add_argument("--operation", default="DEBIT")
a = p.parse_args()
request = {"transaction_id": a.transaction_id, "account_id": a.account_id, "amount": a.amount,
           "currency": a.currency, "operation": a.operation}
with socket.create_connection((a.host, a.port), timeout=3) as s:
    s.settimeout(3)
    s.sendall((json.dumps(request) + "\n").encode("ascii"))
    print(json.loads(s.recv(2048).decode("utf-8")))

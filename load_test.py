#!/usr/bin/env python3
import argparse
import json
import statistics
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor

parser = argparse.ArgumentParser(description="Carga HTTP do IronBridge")
parser.add_argument("--url", default="http://127.0.0.1:8080/transactions")
parser.add_argument("--requests", type=int, default=100)
parser.add_argument("--workers", type=int, default=10)
args = parser.parse_args()

def request(i):
    body = json.dumps({"transaction_id": f"LOAD{i:012d}", "account_id": "123456789012", "amount": "1.00", "currency": "BRL", "operation": "DEBIT"}).encode()
    req = urllib.request.Request(args.url, data=body, headers={"Content-Type": "application/json", "X-Correlation-ID": f"load-{i}"})
    started = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=10) as response:
            response.read(); return response.status, time.perf_counter() - started
    except Exception:
        return 0, time.perf_counter() - started

started = time.perf_counter()
with ThreadPoolExecutor(max_workers=args.workers) as pool:
    results = list(pool.map(request, range(args.requests)))
elapsed = time.perf_counter() - started
latencies = sorted(latency for _, latency in results)
ok = sum(status == 200 for status, _ in results)
p95 = latencies[min(len(latencies) - 1, int(len(latencies) * .95))] * 1000
print(f"requests={args.requests} workers={args.workers} ok={ok} errors={args.requests-ok} rps={args.requests/elapsed:.2f} p50_ms={statistics.median(latencies)*1000:.2f} p95_ms={p95:.2f}")
raise SystemExit(0 if ok == args.requests else 1)

#!/usr/bin/env python3
"""REST adapter do IronBridge: HTTP/JSON -> gateway TCP binário."""
import argparse
from contextlib import nullcontext
import hashlib
import hmac
import json
import os
import sqlite3
import socket
import threading
import uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

class IdempotencyConflict(ValueError):
    pass

class Handler(BaseHTTPRequestHandler):
    gateway_host = "127.0.0.1"
    gateway_port = 3900
    api_token = ""
    metrics = {"requests_total": 0, "requests_ok": 0, "requests_error": 0}
    metrics_lock = threading.Lock()
    request_slots = threading.BoundedSemaphore(128)
    max_body = 8192
    db_path = ""
    db_lock = threading.Lock()
    idempotency_serial = threading.Lock()

    @classmethod
    def idempotency_lookup(cls, key, request_hash):
        if not cls.db_path or not key:
            return None
        with cls.db_lock, sqlite3.connect(cls.db_path, timeout=5) as db:
            db.execute("CREATE TABLE IF NOT EXISTS idempotency (key TEXT PRIMARY KEY, request_hash TEXT NOT NULL, response TEXT NOT NULL, status INTEGER NOT NULL, correlation_id TEXT NOT NULL)")
            row = db.execute("SELECT request_hash, response, status, correlation_id FROM idempotency WHERE key = ?", (key,)).fetchone()
        if not row:
            return None
        if row[0] != request_hash:
            raise IdempotencyConflict("Idempotency-Key já usada com outro payload")
        return json.loads(row[1]), row[2], row[3]

    @classmethod
    def idempotency_store(cls, key, request_hash, response, status, correlation_id):
        if not cls.db_path or not key:
            return
        with cls.db_lock, sqlite3.connect(cls.db_path, timeout=5) as db:
            db.execute("CREATE TABLE IF NOT EXISTS idempotency (key TEXT PRIMARY KEY, request_hash TEXT NOT NULL, response TEXT NOT NULL, status INTEGER NOT NULL, correlation_id TEXT NOT NULL)")
            db.execute("INSERT OR IGNORE INTO idempotency VALUES (?, ?, ?, ?, ?)", (key, request_hash, json.dumps(response, ensure_ascii=False), status, correlation_id))
            db.commit()

    def send_json(self, response, status, correlation_id):
        encoded = json.dumps(response, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.send_header("X-Correlation-ID", correlation_id)
        self.end_headers()
        self.wfile.write(encoded)

    def authorized(self):
        if not self.api_token:
            return True
        return hmac.compare_digest(self.headers.get("Authorization", ""), "Bearer " + self.api_token)

    def do_GET(self):
        if self.path == "/health":
            self.send_json({"status": "UP"}, 200, self.headers.get("X-Correlation-ID", str(uuid.uuid4())))
            return
        if self.path == "/ready":
            correlation_id = self.headers.get("X-Correlation-ID", str(uuid.uuid4()))
            try:
                with socket.create_connection((self.gateway_host, self.gateway_port), timeout=1):
                    self.send_json({"status": "READY"}, 200, correlation_id)
            except OSError:
                self.send_json({"status": "NOT_READY"}, 503, correlation_id)
            return
        if self.path == "/metrics":
            with self.metrics_lock:
                values = dict(self.metrics)
            body = "\n".join("ironbridge_http_{} {}".format(k, v) for k, v in values.items()) + "\n"
            self.send_response(200); self.send_header("Content-Type", "text/plain; version=0.0.4"); self.end_headers(); self.wfile.write(body.encode())
            return
        self.send_error(404)

    def _recv_line(self, upstream, limit=2048):
        data = bytearray()
        while len(data) < limit:
            chunk = upstream.recv(min(512, limit - len(data)))
            if not chunk:
                break
            data.extend(chunk)
            if b"\n" in chunk:
                break
        if b"\n" not in data:
            raise ValueError("resposta do gateway incompleta")
        return bytes(data).split(b"\n", 1)[0]

    def do_POST(self):
        if not self.request_slots.acquire(blocking=False):
            correlation_id = self.headers.get("X-Correlation-ID", str(uuid.uuid4()))
            self.send_json({"status": "ER", "code": "503", "message": "API sobrecarregada"}, 503, correlation_id)
            return
        try:
            self._do_POST()
        finally:
            self.request_slots.release()

    def _do_POST(self):
        correlation_id = self.headers.get("X-Correlation-ID", str(uuid.uuid4()))
        with self.metrics_lock:
            self.metrics["requests_total"] += 1
        if not self.authorized():
            with self.metrics_lock: self.metrics["requests_error"] += 1
            self.send_json({"status": "ER", "code": "401", "message": "Não autorizado"}, 401, correlation_id)
            return
        if self.path != "/transactions":
            self.send_error(404, "use POST /transactions")
            return
        try:
            if self.headers.get("Content-Type", "").split(";", 1)[0].strip().lower() != "application/json":
                raise ValueError("Content-Type deve ser application/json")
            length = int(self.headers.get("Content-Length", "-1"))
            if length < 0 or length > self.max_body:
                raise ValueError("corpo HTTP ausente ou maior que o limite")
            raw_body = self.rfile.read(length)
            request_hash = hashlib.sha256(raw_body).hexdigest()
            idempotency_key = self.headers.get("Idempotency-Key", "")
            if len(idempotency_key) > 128:
                raise ValueError("Idempotency-Key maior que o limite")
            request = json.loads(raw_body.decode("utf-8"))
            request.setdefault("correlation_id", correlation_id)
            guard = self.idempotency_serial if self.db_path and idempotency_key else nullcontext()
            with guard:
                cached = self.idempotency_lookup(idempotency_key, request_hash)
                if cached:
                    response, status, cached_correlation = cached
                    self.send_json(response, status, cached_correlation)
                    return
                with socket.create_connection((self.gateway_host, self.gateway_port), timeout=3) as upstream:
                    upstream.settimeout(3)
                    upstream.sendall((json.dumps(request, ensure_ascii=False) + "\n").encode("utf-8"))
                    response = json.loads(self._recv_line(upstream).decode("utf-8"))
                status = 200 if response.get("status") == "OK" else 422
                self.idempotency_store(idempotency_key, request_hash, response, status, correlation_id)
            with self.metrics_lock: self.metrics["requests_ok" if status == 200 else "requests_error"] += 1
        except IdempotencyConflict as exc:
            response, status = {"status": "ER", "code": "409", "message": str(exc)}, 409
            with self.metrics_lock: self.metrics["requests_error"] += 1
        except (ValueError, json.JSONDecodeError):
            response, status = {"status": "ER", "code": "400", "message": "JSON HTTP inválido"}, 400
            with self.metrics_lock: self.metrics["requests_error"] += 1
        except (OSError, TimeoutError):
            response, status = {"status": "ER", "code": "503", "message": "Gateway indisponível"}, 503
            with self.metrics_lock: self.metrics["requests_error"] += 1
        self.send_json(response, status, correlation_id)

    def log_message(self, format, *args):
        return

parser = argparse.ArgumentParser()
parser.add_argument("--host", default="127.0.0.1")
parser.add_argument("--port", type=int, default=8080)
parser.add_argument("--gateway-host", default="127.0.0.1")
parser.add_argument("--gateway-port", type=int, default=3900)
parser.add_argument("--token", default=os.environ.get("IRONBRIDGE_API_TOKEN", ""))
parser.add_argument("--tls-cert", default="")
parser.add_argument("--tls-key", default="")
parser.add_argument("--db", default=os.environ.get("IRONBRIDGE_DB", ""), help="SQLite para Idempotency-Key")
args = parser.parse_args()
Handler.gateway_host, Handler.gateway_port = args.gateway_host, args.gateway_port
Handler.api_token = args.token
Handler.db_path = args.db
if Handler.db_path:
    os.makedirs(os.path.dirname(os.path.abspath(Handler.db_path)), exist_ok=True)
server = ThreadingHTTPServer((args.host, args.port), Handler)
if bool(args.tls_cert) != bool(args.tls_key):
    raise SystemExit("--tls-cert e --tls-key devem ser usados juntos")
if args.tls_cert:
    import ssl
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(args.tls_cert, args.tls_key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
server.serve_forever()

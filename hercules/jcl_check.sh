#!/usr/bin/env bash
set -eu
root="$(cd "$(dirname "$0")/mvs-tk5/mvs-tk5" && pwd)"
tmpdir="$(mktemp -d)"
job="IBP$(printf '%05d' "$RANDOM")"
log="/tmp/ironbridge-${job}.log"
cleanup() {
    if [ -n "${pgid:-}" ]; then
        hercules_pid="$(pgrep -s "$pgid" -f 'hercules -d -f conf/tk5.cnf' | head -1 || true)"
        if [ -n "$hercules_pid" ] && [ -w "/proc/$hercules_pid/fd/0" ]; then
            printf 'exit\n' >"/proc/$hercules_pid/fd/0" 2>/dev/null || true
        fi
        for _ in $(seq 1 10); do
            kill -0 "$hercules_pid" 2>/dev/null || break
            sleep 1
        done
        # Algumas versões do Hercules imprimem "shutdown complete" mas deixam
        # uma thread de dispositivo viva; limite o fallback ao PID do guest.
        if [ -n "$hercules_pid" ] && kill -0 "$hercules_pid" 2>/dev/null; then
            kill -KILL "$hercules_pid" 2>/dev/null || true
        fi
        kill -TERM -- -"$pgid" 2>/dev/null || true
    fi
    rm -rf "$tmpdir"
}
trap cleanup EXIT
chmod +x "$root/mvs" "$root/mvs_ipl" "$root/hercules/linux/64/bin/hercules" 2>/dev/null || true
sed "s/IBP002/$job/g" jcl/ironbridge-proof.jcl >"$tmpdir/job.jcl"
setsid bash -c "ulimit -t 120 -v 2097152; cd '$root' && MODE=CONSOLE exec nice -n 10 ./mvs >'$log' 2>&1" & pgid=$!
for _ in $(seq 1 45); do
    if grep -q 'IEE041I THE SYSTEM LOG IS NOW ACTIVE' "$log" 2>/dev/null && \
       python3 -c 'import socket; s=socket.create_connection(("127.0.0.1",3505), .2); s.close()' 2>/dev/null; then break; fi
    sleep 1
done
grep -q 'IEE041I THE SYSTEM LOG IS NOW ACTIVE' "$log"
python3 -c 'import socket; s=socket.create_connection(("127.0.0.1",3505), 2); s.close()'
sleep 10
python3 - "$tmpdir/job.jcl" <<'PY'
import socket, sys
with open(sys.argv[1], 'rb') as stream:
    data = stream.read()
with socket.create_connection(('127.0.0.1', 3505), timeout=10) as sock:
    sock.sendall(data)
    sock.shutdown(socket.SHUT_WR)
PY
for _ in $(seq 1 45); do
    if grep -a -Eq "$job.*IEBGENER.*RC= 0000" "$root/log/hardcopy.log" 2>/dev/null; then
        grep -a -E "$job.*(STARTED|IEBGENER|ENDED)" "$root/log/hardcopy.log" | tail -5
        echo "teste JCL dentro do MVS: OK"
        exit 0
    fi
    sleep 1
done
grep -a -E "$job|HASP301" "$root/log/hardcopy.log" | tail -20 || true
exit 1

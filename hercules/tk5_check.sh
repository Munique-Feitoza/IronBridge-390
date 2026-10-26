#!/usr/bin/env bash
set -eu
root="$(cd "$(dirname "$0")/mvs-tk5/mvs-tk5" && pwd)"
log="${IRONBRIDGE_TK5_LOG:-$root/log/ironbridge-tk5.log}"
mkdir -p "$(dirname "$log")"
chmod +x "$root/mvs" "$root/mvs_ipl" "$root/hercules/linux/64/bin/hercules" 2>/dev/null || true
stop_guest() {
    if [ -n "${pid:-}" ]; then
        hercules_pid="$(pgrep -s "$pid" -f 'hercules -d -f conf/tk5.cnf' | head -1 || true)"
        if [ -n "$hercules_pid" ] && [ -w "/proc/$hercules_pid/fd/0" ]; then
            printf 'exit\n' >"/proc/$hercules_pid/fd/0" 2>/dev/null || true
        fi
        for _ in $(seq 1 10); do
            [ -z "$hercules_pid" ] || ! kill -0 "$hercules_pid" 2>/dev/null && break
            sleep 1
        done
        if [ -n "$hercules_pid" ] && kill -0 "$hercules_pid" 2>/dev/null; then
            kill -KILL "$hercules_pid" 2>/dev/null || true
        fi
        kill -TERM -- -"$pid" 2>/dev/null || true
    fi
}
trap stop_guest EXIT
set +e
setsid bash -c "ulimit -t 90 -v 2097152; cd '$root' && MODE=CONSOLE exec nice -n 10 ./mvs >'$log' 2>&1" & pid=$!
for _ in $(seq 1 30); do
    if grep -Eq '\$HASP493 JES2  (QUICK|WARM)-START IS IN PROGRESS' "$log" 2>/dev/null; then
        echo "MVS 3.8J TK5 iniciou o IPL e o JES2"
        stop_guest
        wait "$pid" 2>/dev/null || true
        exit 0
    fi
    if ! kill -0 "$pid" 2>/dev/null; then
        cat "$log"
        exit 1
    fi
    sleep 1
done
echo "timeout aguardando IPL do TK5"
tail -120 "$log"
stop_guest
wait "$pid" 2>/dev/null || true
exit 1

#!/usr/bin/env bash
set -eu

root="$(cd "$(dirname "$0")/.." && pwd)"
z390_root="$root/.tools/z390"
if [ ! -x "$z390_root/bash/cblclg" ] || [ ! -f "$z390_root/z390.jar" ]; then
    echo "z390 ausente: consulte docs/z390-toolchain.md" >&2
    exit 2
fi

# z390 V1.8.05 tem scripts antigos que não protegem caminhos com espaços.
link="/tmp/ironbridge-390"
ln -sfn "$root" "$link"
trap 'rm -f "$link"' EXIT
cd "$link"

# Limites aplicam-se também aos processos Java filhos.
ulimit -t 120
ulimit -v 6291456
timeout --foreground --kill-after=10s 120s \
    bash .tools/z390/bash/cblclg cobol/trx390.cbl
echo "teste zCOBOL/z390: OK"

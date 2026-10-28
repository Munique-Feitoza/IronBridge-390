CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDLIBS ?= -pthread

COMMON = protocol.c ebcdic.c packed_decimal.c copybook.c
SCHEMA = copybook_parser.c

.PHONY: all clean test asan-test hardening-test cobol cobol-backend schema unit-test api-test api-test-cobol java java-test load-test security-test hercules-check hercules-mvs-check hercules-jcl-check z390-test

COBC ?= cobc

all: ironbridge backend-emulator

ironbridge: gateway.c $(COMMON) $(SCHEMA) ironbridge.h
	$(CC) $(CFLAGS) -o $@ gateway.c $(COMMON) $(SCHEMA) $(LDLIBS)

backend-emulator: backend_emulator.c $(COMMON) $(SCHEMA) ironbridge.h
	$(CC) $(CFLAGS) -o $@ backend_emulator.c $(COMMON) $(SCHEMA)

copybook-inspect: copybook_inspect.c $(SCHEMA) ironbridge.h
	$(CC) $(CFLAGS) -o $@ copybook_inspect.c $(SCHEMA)

unit-test: unit_test.c ebcdic.c packed_decimal.c ironbridge.h
	$(CC) $(CFLAGS) -o $@ unit_test.c ebcdic.c packed_decimal.c

test: all unit-test cobol-backend
	./unit-test
	./test.sh
	./cobol_api_test.sh
	./security_test.sh
	./hardening_test.sh
	$(MAKE) java-test

java:
	javac --release 17 -d java java/IronBridgeClient.java

java-test: all java
	./backend-emulator 13921 >/tmp/ironbridge-java-backend.log 2>&1 & backend_pid=$$!; \
	./ironbridge 13920 127.0.0.1 13921 >/tmp/ironbridge-java-gateway.log 2>&1 & gateway_pid=$$!; \
	trap 'kill $$gateway_pid $$backend_pid 2>/dev/null || true' EXIT; \
	for i in $$(seq 1 20); do java -cp java IronBridgeClient 127.0.0.1 13920 TRXJAVA00000001 123456789012 125.37 BRL DEBIT >/tmp/ironbridge-java-response.json 2>/dev/null && break || sleep .05; done; \
	grep -q '"status":"OK"' /tmp/ironbridge-java-response.json; \
	echo "teste Java: OK"

hardening-test: all
	./hardening_test.sh

asan-test:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' all unit-test
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./unit-test
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./test.sh

api-test: all
	./api_test.sh

load-test:
	python3 load_test.py

security-test: all
	./security_test.sh

hercules-check:
	timeout -k 1 3 hercules -f hercules/ironbridge.cnf -n </dev/null >/tmp/ironbridge-hercules.log 2>&1
	grep -q 'Configuration released' /tmp/ironbridge-hercules.log
	@echo "teste de configuração Hercules: OK"

hercules-mvs-check:
	timeout --foreground --kill-after=15s 150s ./hercules/tk5_check.sh

hercules-jcl-check:
	timeout --foreground --kill-after=15s 180s ./hercules/jcl_check.sh

z390-test:
	./z390/check.sh

cobol-api-test: all cobol-backend
	./cobol_api_test.sh

schema: copybook-inspect
	./copybook-inspect copybook.cpy

cobol:
	@if command -v $(COBC) >/dev/null 2>&1; then $(COBC) -x -free -o transaction cobol/transaction.cob; else echo 'cobc não instalado; fonte disponível em cobol/transaction.cob'; fi

cobol-backend:
	$(COBC) -x -free -o cobol-backend cobol/backend_server.cob cobol_socket_shim.c ebcdic.c

clean:
	rm -f ironbridge backend-emulator copybook-inspect unit-test transaction cobol-backend java/*.class

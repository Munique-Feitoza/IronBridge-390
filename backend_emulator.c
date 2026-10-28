#include "ironbridge.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void process(int fd) {
    uint8_t payload[IB_MAX_FRAME], type; uint16_t len; ib_request_t r; ib_response_t out = {0};
    if (ib_read_frame(fd, &type, payload, &len, sizeof payload) != 0 || type != 1 || ib_decode_request(payload, len, &r) != 0) return;
    strncpy(out.transaction_id, r.transaction_id, 16); out.transaction_id[16] = 0;
    if (r.amount_cents <= 0) { strcpy(out.status, "ER"); strcpy(out.code, "422"); strcpy(out.message, "Valor deve ser maior que zero"); }
    else if (strcmp(r.currency, "BRL") != 0) { strcpy(out.status, "ER"); strcpy(out.code, "422"); strcpy(out.message, "Moeda nao suportada"); }
    else if (strcmp(r.operation, "DEBIT") != 0 && strcmp(r.operation, "CRED") != 0) { strcpy(out.status, "ER"); strcpy(out.code, "422"); strcpy(out.message, "Operacao invalida"); }
    else { strcpy(out.status, "OK"); strcpy(out.code, "000"); strcpy(out.message, "Transacao validada pelo core legado"); }
    size_t n; if (ib_encode_response(&out, payload, sizeof payload, &n) == 0) ib_write_frame(fd, 2, payload, (uint16_t)n);
}

int main(int argc, char **argv) {
    int port = argc > 1 ? atoi(argv[1]) : 3901; int server = socket(AF_INET, SOCK_STREAM, 0); int yes = 1; setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY); addr.sin_port = htons((uint16_t)port);
    if (server < 0 || bind(server, (struct sockaddr *)&addr, sizeof addr) < 0 || listen(server, 16) < 0) { perror("backend-emulator"); return 1; }
    fprintf(stderr, "backend-emulator ouvindo em %d\n", port);
    for (;;) { int client = accept(server, NULL, NULL); if (client >= 0) { process(client); close(client); } }
}

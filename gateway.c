#include "ironbridge.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <pthread.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static int connect_backend(const char *host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0); struct sockaddr_in a = {0};
    if (fd < 0) return -1;
    a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &a.sin_addr) != 1) { close(fd); return -1; }
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) { close(fd); return -1; }
    int result = connect(fd, (struct sockaddr *)&a, sizeof a);
    if (result < 0 && errno == EINPROGRESS) {
        struct pollfd pfd = {.fd = fd, .events = POLLOUT};
        result = poll(&pfd, 1, 3000);
        if (result > 0) {
            int error = 0; socklen_t length = sizeof error;
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error != 0) result = -1;
            else result = 0;
        } else result = -1;
    }
    if (result < 0 || fcntl(fd, F_SETFL, flags) < 0) { close(fd); return -1; }
    struct timeval tv = {3, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    return fd;
}

typedef struct {
    const char *backend_host;
    int backend_port;
    const ib_copybook_t *schema;
    const ib_field_map_t *mapping;
} gateway_config_t;
typedef struct {
    int client;
    gateway_config_t config;
} client_job_t;

static _Atomic unsigned long requests_total;
static _Atomic unsigned long requests_ok;
static _Atomic unsigned long requests_error;
static _Atomic unsigned long backend_errors;
static _Atomic unsigned long requests_inflight;
#define IB_MAX_INFLIGHT 128u

static int send_all(int fd, const void *buffer, size_t length) {
    const uint8_t *p = buffer; size_t sent = 0;
    while (sent < length) {
        ssize_t n = send(fd, p + sent, length - sent, 0);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static void log_event(const char *event, const ib_request_t *request, const char *status) {
    fprintf(stderr, "{\"event\":\"%s\",\"correlation_id\":\"%s\",\"transaction_id\":\"%s\",\"status\":\"%s\"}\n",
            event, request ? request->correlation_id : "-", request ? request->transaction_id : "-", status ? status : "-");
}

static void serve_client(int client, const gateway_config_t *config) {
    atomic_fetch_add(&requests_total, 1);
    unsigned long active = atomic_fetch_add(&requests_inflight, 1) + 1;
    if (active > IB_MAX_INFLIGHT) {
        static const char busy[] = "{\"status\":\"ER\",\"code\":\"503\",\"message\":\"Gateway sobrecarregado\"}";
        (void)send_all(client, busy, sizeof busy - 1);
        atomic_fetch_sub(&requests_inflight, 1); atomic_fetch_add(&requests_error, 1); return;
    }
    struct timeval tv = {3, 0};
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    char line[2048]; size_t used = 0;
    while (used + 1 < sizeof line) {
        ssize_t n = recv(client, line + used, 1, 0);
        if (n <= 0) { atomic_fetch_sub(&requests_inflight, 1); return; }
        if (line[used++] == '\n') break;
    }
    line[used] = 0;
    if (!used || line[used - 1] != '\n') { atomic_fetch_sub(&requests_inflight, 1); return; }
    ib_request_t request = {0}; char error[128]; uint8_t payload[IB_MAX_FRAME]; size_t payload_len;
    ib_response_t response = {0};
    if (ib_parse_json(line, &request, error, sizeof error) != 0) {
        strcpy(response.status, "ER"); strcpy(response.code, "400"); strcpy(response.message, error);
    } else if ((config->schema ? ib_encode_request_schema_map(&request, config->schema, config->mapping, payload, sizeof payload, &payload_len) :
               ib_encode_request(&request, payload, sizeof payload, &payload_len)) != 0) {
        strcpy(response.status, "ER"); strcpy(response.code, "422"); strcpy(response.message, "Payload incompatível com o copybook");
        strncpy(response.transaction_id, request.transaction_id, 16); response.transaction_id[16] = 0;
    } else {
        int backend = connect_backend(config->backend_host, config->backend_port); uint8_t reply[IB_MAX_FRAME]; uint8_t type; uint16_t len;
        if (backend < 0 || ib_write_frame(backend, 1, payload, (uint16_t)payload_len) != 0 || ib_read_frame(backend, &type, reply, &len, sizeof reply) != 0 || type != 2 || ib_decode_response(reply, len, &response) != 0) {
            strcpy(response.status, "ER"); strcpy(response.code, "503"); strcpy(response.message, "Backend indisponível ou resposta inválida");
            strncpy(response.transaction_id, request.transaction_id, 16); response.transaction_id[16] = 0;
            atomic_fetch_add(&backend_errors, 1);
        }
        if (backend >= 0) close(backend);
    }
    char json[512]; int out = ib_response_json(&response, json, sizeof json);
    if (out > 0) { send_all(client, json, (size_t)out); send_all(client, "\n", 1); }
    if (strcmp(response.status, "OK") == 0) atomic_fetch_add(&requests_ok, 1); else atomic_fetch_add(&requests_error, 1);
    log_event("request.complete", &request, response.status);
    atomic_fetch_sub(&requests_inflight, 1);
}

static void *client_worker(void *arg) {
    client_job_t *job = arg;
    serve_client(job->client, &job->config); close(job->client); free(job); return NULL;
}

int main(int argc, char **argv) {
    ib_copybook_t schema = {0}; const ib_copybook_t *schema_ptr = NULL;
    ib_field_map_t mapping; ib_copybook_mapping_default(&mapping);
    int port = argc > 1 ? atoi(argv[1]) : 3900; const char *host = argc > 2 ? argv[2] : "127.0.0.1"; int backend_port = argc > 3 ? atoi(argv[3]) : 3901;
    if (argc > 4) {
        char error[128] = {0};
        if (ib_copybook_load(argv[4], &schema, error, sizeof error) != 0 || schema.record_size != IB_REQUEST_PAYLOAD_SIZE) {
            fprintf(stderr, "copybook incompatível: %s\n", error[0] ? error : "tamanho diferente do contrato"); return 1;
        }
        fprintf(stderr, "copybook carregado: %s (%zu bytes)\n", argv[4], schema.record_size);
        schema_ptr = &schema;
    }
    if (argc > 5) {
        char error[128] = {0};
        if (ib_copybook_mapping_load(argv[5], &mapping, error, sizeof error) != 0) {
            fprintf(stderr, "mapeamento incompatível: %s\n", error); return 1;
        }
        fprintf(stderr, "mapeamento carregado: %s\n", argv[5]);
    }
    signal(SIGPIPE, SIG_IGN); int server = socket(AF_INET, SOCK_STREAM, 0); int yes = 1; setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    struct sockaddr_in addr = {0}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = htonl(INADDR_ANY); addr.sin_port = htons((uint16_t)port);
    if (server < 0 || bind(server, (struct sockaddr *)&addr, sizeof addr) < 0 || listen(server, 16) < 0) { perror("ironbridge"); return 1; }
    gateway_config_t config = {host, backend_port, schema_ptr, &mapping};
    fprintf(stderr, "ironbridge ouvindo em %d; backend %s:%d; workers=pthread\n", port, host, backend_port);
    for (;;) {
        int client = accept(server, NULL, NULL); if (client < 0) continue;
        client_job_t *job = malloc(sizeof *job); if (!job) { close(client); continue; }
        job->client = client; job->config = config;
        pthread_t thread; if (pthread_create(&thread, NULL, client_worker, job) != 0) { close(client); free(job); continue; }
        pthread_detach(thread);
    }
}

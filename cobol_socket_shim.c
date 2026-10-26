#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static int transfer(int fd, void *buffer, int length, int sending) {
    int done = 0;
    while (done < length) {
        ssize_t n = sending ? send(fd, (char *)buffer + done, (size_t)(length - done), 0)
                            : recv(fd, (char *)buffer + done, (size_t)(length - done), 0);
        if (n == 0) return -1;
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        done += (int)n;
    }
    return done;
}

int ib_socket_listen(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0), yes = 1;
    struct sockaddr_in address = {0};
    if (fd < 0) return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_ANY); address.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&address, sizeof address) < 0 || listen(fd, 16) < 0) { close(fd); return -1; }
    return fd;
}

int ib_socket_accept(int server) { return accept(server, NULL, NULL); }
int ib_socket_recv_exact(int client, void *buffer, int length) { return transfer(client, buffer, length, 0); }
int ib_socket_send_exact(int client, void *buffer, int length) { return transfer(client, buffer, length, 1); }
int ib_socket_close(int fd) { return close(fd); }

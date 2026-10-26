#include "ironbridge.h"
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

static int transfer(int fd, void *buf, size_t len, int write_mode) {
    size_t done = 0;
    while (done < len) {
        ssize_t n = write_mode ? send(fd, (char *)buf + done, len - done, 0)
                               : recv(fd, (char *)buf + done, len - done, 0);
        if (n == 0) return -1;
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        done += (size_t)n;
    }
    return 0;
}

int ib_write_frame(int fd, uint8_t type, const uint8_t *payload, uint16_t length) {
    uint8_t header[IB_HEADER_SIZE] = {'I', 'B', 1, type,
                                      (uint8_t)(length >> 8), (uint8_t)length};
    return transfer(fd, header, sizeof header, 1) || transfer(fd, (void *)payload, length, 1) ? -1 : 0;
}

int ib_read_frame(int fd, uint8_t *type, uint8_t *payload, uint16_t *length, size_t cap) {
    uint8_t header[IB_HEADER_SIZE];
    if (transfer(fd, header, sizeof header, 0) != 0 || header[0] != 'I' || header[1] != 'B' || header[2] != 1)
        return -1;
    uint16_t len = (uint16_t)(((uint16_t)header[4] << 8) | header[5]);
    if (len > cap) return -1;
    if (transfer(fd, payload, len, 0) != 0) return -1;
    *type = header[3]; *length = len;
    return 0;
}

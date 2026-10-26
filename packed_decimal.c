#include <stdint.h>
#include <stddef.h>

int ib_pack_decimal(int64_t value, uint8_t *out, size_t bytes, unsigned digits) {
    int negative = value < 0;
    uint64_t magnitude = negative ? (uint64_t)(-(value + 1)) + 1u : (uint64_t)value;
    uint64_t limit = 1;
    for (unsigned i = 0; i < digits; ++i) limit *= 10;
    if (magnitude >= limit || bytes * 2 < (size_t)digits + 1) return -1;
    for (size_t i = 0; i < bytes; ++i) out[i] = 0;
    for (int pos = (int)bytes * 2 - 2; pos >= 0; --pos) {
        uint8_t d = (uint8_t)(magnitude % 10); magnitude /= 10;
        if ((pos & 1) == 0) out[pos / 2] |= (uint8_t)(d << 4);
        else out[pos / 2] |= d;
    }
    out[bytes - 1] = (uint8_t)((out[bytes - 1] & 0xf0) | (negative ? 0x0d : 0x0c));
    return 0;
}

int ib_unpack_decimal(const uint8_t *in, size_t bytes, int64_t *value) {
    int64_t result = 0;
    for (size_t i = 0; i < bytes; ++i) {
        uint8_t hi = in[i] >> 4, lo = in[i] & 0x0f;
        unsigned count = (i == bytes - 1) ? 1u : 2u;
        if (hi > 9 || (count == 2 && lo > 9)) return -1;
        if (i != bytes - 1) result = result * 100 + hi * 10 + lo;
        else result = result * 10 + hi;
    }
    uint8_t sign = in[bytes - 1] & 0x0f;
    if (sign != 0x0c && sign != 0x0d && sign != 0x0f) return -1;
    *value = sign == 0x0d ? -result : result;
    return 0;
}

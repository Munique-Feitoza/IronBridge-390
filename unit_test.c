#include "ironbridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int ib_ascii_to_ebcdic(const char *, unsigned char *, size_t);
void ib_ebcdic_to_ascii(const unsigned char *, char *, size_t);
int ib_pack_decimal(long long, unsigned char *, size_t, unsigned);
int ib_unpack_decimal(const unsigned char *, size_t, long long *);

int main(void) {
    unsigned char bytes[16]; char text[16]; long long value;
    assert(ib_ascii_to_ebcdic("ABC 123", bytes, 7) == 0);
    ib_ebcdic_to_ascii(bytes, text, 7); text[7] = 0;
    assert(strcmp(text, "ABC 123") == 0);
    assert(ib_ascii_to_ebcdic("á", bytes, 2) != 0);
    assert(ib_pack_decimal(12537, bytes, 6, 11) == 0);
    assert(ib_unpack_decimal(bytes, 6, &value) == 0 && value == 12537);
    assert(ib_pack_decimal(-99, bytes, 6, 11) == 0);
    assert(ib_unpack_decimal(bytes, 6, &value) == 0 && value == -99);
    assert(ib_pack_decimal(100000000000LL, bytes, 6, 11) != 0);
    puts("testes unitarios EBCDIC/COMP-3: OK");
    return 0;
}

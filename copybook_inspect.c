#include "ironbridge.h"
#include <stdio.h>

int main(int argc, char **argv) {
    ib_copybook_t schema; char error[128] = {0};
    if (argc != 2 || ib_copybook_load(argv[1], &schema, error, sizeof error) != 0) {
        fprintf(stderr, "uso: %s arquivo.cpy (%s)\n", argv[0], error[0] ? error : "argumentos invalidos"); return 1;
    }
    printf("fields=%zu record_size=%zu\n", schema.count, schema.record_size);
    for (size_t i = 0; i < schema.count; ++i) {
        const ib_field_t *f = &schema.fields[i];
        printf("%-12s offset=%-3zu size=%-2zu type=%s", f->name, f->offset, f->size, f->kind == IB_FIELD_TEXT ? "PIC X" : "PIC 9");
        if (f->kind == IB_FIELD_DECIMAL) printf(" digits=%u scale=%u %s", f->digits, f->scale, f->packed ? "COMP-3" : "DISPLAY");
        putchar('\n');
    }
    return 0;
}

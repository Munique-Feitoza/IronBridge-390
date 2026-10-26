#include "ironbridge.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim_token(char *s);

void ib_copybook_mapping_default(ib_field_map_t *map) {
    snprintf(map->transaction_id, sizeof map->transaction_id, "TRX-ID");
    snprintf(map->account_id, sizeof map->account_id, "ACCOUNT-ID");
    snprintf(map->amount, sizeof map->amount, "AMOUNT");
    snprintf(map->currency, sizeof map->currency, "CURRENCY-CODE");
    snprintf(map->operation, sizeof map->operation, "OPERATION");
}

static int mapping_value(char *line, const char *key, char *out, size_t cap) {
    size_t n = strlen(key);
    if (strncmp(line, key, n) != 0 || line[n] != '=') return 0;
    char *value = line + n + 1;
    while (isspace((unsigned char)*value)) ++value;
    trim_token(value);
    if (!*value || strlen(value) >= cap) return -1;
    snprintf(out, cap, "%s", value);
    return 1;
}

int ib_copybook_mapping_load(const char *path, ib_field_map_t *map, char *error, size_t error_cap) {
    FILE *file = fopen(path, "r"); char line[256];
    if (!file) { snprintf(error, error_cap, "nao foi possivel abrir o mapeamento"); return -1; }
    ib_copybook_mapping_default(map);
    while (fgets(line, sizeof line, file)) {
        char *comment = strstr(line, "#"); if (comment) *comment = 0;
        char *p = line; while (isspace((unsigned char)*p)) ++p;
        if (!*p) continue;
        int rc = 0;
        if ((rc = mapping_value(p, "transaction_id", map->transaction_id, sizeof map->transaction_id)) > 0) {}
        else if ((rc = mapping_value(p, "account_id", map->account_id, sizeof map->account_id)) > 0) {}
        else if ((rc = mapping_value(p, "amount", map->amount, sizeof map->amount)) > 0) {}
        else if ((rc = mapping_value(p, "currency", map->currency, sizeof map->currency)) > 0) {}
        else if ((rc = mapping_value(p, "operation", map->operation, sizeof map->operation)) > 0) {}
        else { fclose(file); snprintf(error, error_cap, "linha de mapeamento invalida"); return -1; }
        if (rc < 0) { fclose(file); snprintf(error, error_cap, "valor de mapeamento invalido"); return -1; }
    }
    fclose(file);
    return 0;
}

static void trim_token(char *s) {
    size_t n = strlen(s); while (n && (s[n - 1] == '.' || isspace((unsigned char)s[n - 1]))) s[--n] = 0;
}

static char *find_ci(char *text, const char *needle) {
    size_t n = strlen(needle);
    for (char *p = text; *p; ++p) {
        size_t i = 0;
        while (i < n && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) ++i;
        if (i == n) return p;
    }
    return NULL;
}

static int has_ci(const char *text, const char *needle) {
    return find_ci((char *)text, needle) != NULL;
}

static int digits_in_pic(const char *pic, unsigned *digits, unsigned *scale) {
    const char *v = strchr(pic, 'V'); if (!v) v = strchr(pic, 'v');
    const char *end = v ? v : pic + strlen(pic); unsigned integer = 0, fraction = 0;
    const char *p = strchr(pic, '(');
    if (p && p < end) integer = (unsigned)strtoul(p + 1, NULL, 10);
    else for (p = pic; p < end; ++p) if (*p == '9') ++integer;
    if (v) {
        p = strchr(v, '(');
        if (p) fraction = (unsigned)strtoul(p + 1, NULL, 10);
        else for (p = v + 1; *p && *p != '.'; ++p) if (*p == '9') ++fraction;
    }
    if (!integer && !fraction) return -1;
    *digits = integer + fraction; *scale = fraction; return 0;
}

int ib_copybook_load(const char *path, ib_copybook_t *schema, char *error, size_t error_cap) {
    FILE *file = fopen(path, "r"); char line[256]; unsigned level; char name[32], pic[64];
    if (!file) { snprintf(error, error_cap, "nao foi possivel abrir o copybook"); return -1; }
    memset(schema, 0, sizeof *schema);
    while (fgets(line, sizeof line, file)) {
        char *comment = find_ci(line, "*>");
        if (comment) *comment = 0;
        char *first = line; while (isspace((unsigned char)*first)) ++first;
        if (*first == '*' || !*first) continue;
        char *marker = find_ci(line, "PIC");
        if (!marker || sscanf(line, "%u %31s", &level, name) != 2) continue;
        if (schema->count >= IB_MAX_FIELDS || sscanf(marker + 3, "%63s", pic) != 1) { fclose(file); return -1; }
        trim_token(name); trim_token(pic);
        ib_field_t *f = &schema->fields[schema->count]; snprintf(f->name, sizeof f->name, "%s", name);
        f->offset = schema->record_size; f->packed = has_ci(marker, "COMP-3");
        char pic_kind = (char)toupper((unsigned char)pic[0]);
        if (pic_kind == 'X') {
            f->kind = IB_FIELD_TEXT; char *open = strchr(pic, '('); f->digits = open ? (unsigned)strtoul(open + 1, NULL, 10) : 1; f->size = f->digits;
        } else if (pic_kind == '9' || pic_kind == 'S') {
            f->kind = IB_FIELD_DECIMAL; if (digits_in_pic(pic, &f->digits, &f->scale) != 0) { fclose(file); return -1; }
            f->size = f->packed ? (f->digits + 2) / 2 : f->digits;
        } else continue;
        schema->record_size += f->size; schema->count++;
    }
    fclose(file); if (!schema->count) { snprintf(error, error_cap, "nenhum campo suportado encontrado"); return -1; }
    return 0;
}

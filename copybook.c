#include "ironbridge.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>

int ib_ascii_to_ebcdic(const char *, uint8_t *, size_t);
void ib_ebcdic_to_ascii(const uint8_t *, char *, size_t);
int ib_pack_decimal(int64_t, uint8_t *, size_t, unsigned);
int ib_unpack_decimal(const uint8_t *, size_t, int64_t *);

static void field_out(char *dst, size_t n, const uint8_t *src, size_t len) {
    ib_ebcdic_to_ascii(src, dst, len); dst[len] = 0;
    while (len && dst[len - 1] == ' ') dst[--len] = 0;
    (void)n;
}

int ib_encode_request(const ib_request_t *r, uint8_t *out, size_t cap, size_t *written) {
    if (cap < IB_REQUEST_PAYLOAD_SIZE || strlen(r->transaction_id) > 16 || strlen(r->account_id) > 12 ||
        strlen(r->currency) != 3 || strlen(r->operation) == 0 || strlen(r->operation) > 5) return -1;
    memset(out, 0x40, IB_REQUEST_PAYLOAD_SIZE);
    if (ib_ascii_to_ebcdic(r->transaction_id, out, 16) || ib_ascii_to_ebcdic(r->account_id, out + 16, 12) ||
        ib_pack_decimal(r->amount_cents, out + 28, 6, 11) || ib_ascii_to_ebcdic(r->currency, out + 34, 3) ||
        ib_ascii_to_ebcdic(r->operation, out + 37, 5)) return -1;
    *written = IB_REQUEST_PAYLOAD_SIZE; return 0;
}

static const ib_field_t *schema_field(const ib_copybook_t *schema, const char *name) {
    if (!schema) return NULL;
    for (size_t i = 0; i < schema->count; ++i)
        if (strcmp(schema->fields[i].name, name) == 0) return &schema->fields[i];
    return NULL;
}

int ib_encode_request_schema_map(const ib_request_t *r, const ib_copybook_t *schema,
                                 const ib_field_map_t *map, uint8_t *out, size_t cap, size_t *written) {
    if (!map) return -1;
    const ib_field_t *trx = schema_field(schema, map->transaction_id);
    const ib_field_t *account = schema_field(schema, map->account_id);
    const ib_field_t *amount = schema_field(schema, map->amount);
    const ib_field_t *currency = schema_field(schema, map->currency);
    const ib_field_t *operation = schema_field(schema, map->operation);
    if (!trx || !account || !amount || !currency || !operation || schema->record_size != IB_REQUEST_PAYLOAD_SIZE || schema->record_size > cap ||
        trx->offset != 0 || trx->kind != IB_FIELD_TEXT || trx->size != 16 || account->offset != 16 || account->kind != IB_FIELD_TEXT || account->size != 12 ||
        amount->offset != 28 || amount->kind != IB_FIELD_DECIMAL || !amount->packed || amount->digits != 11 || amount->scale != 2 || amount->size != 6 ||
        currency->offset != 34 || currency->kind != IB_FIELD_TEXT || currency->size != 3 || operation->offset != 37 || operation->kind != IB_FIELD_TEXT || operation->size != 5 ||
        strlen(r->transaction_id) > trx->size || strlen(r->account_id) > account->size || strlen(r->currency) != currency->size ||
        strlen(r->operation) == 0 || strlen(r->operation) > operation->size)
        return -1;
    memset(out, 0x40, schema->record_size);
    if (ib_ascii_to_ebcdic(r->transaction_id, out + trx->offset, trx->size) ||
        ib_ascii_to_ebcdic(r->account_id, out + account->offset, account->size) ||
        ib_pack_decimal(r->amount_cents, out + amount->offset, amount->size, amount->digits) ||
        ib_ascii_to_ebcdic(r->currency, out + currency->offset, currency->size) ||
        ib_ascii_to_ebcdic(r->operation, out + operation->offset, operation->size)) return -1;
    *written = schema->record_size;
    return 0;
}

int ib_encode_request_schema(const ib_request_t *r, const ib_copybook_t *schema,
                             uint8_t *out, size_t cap, size_t *written) {
    ib_field_map_t map;
    ib_copybook_mapping_default(&map);
    return ib_encode_request_schema_map(r, schema, &map, out, cap, written);
}

int ib_decode_request(const uint8_t *p, size_t len, ib_request_t *r) {
    if (len != IB_REQUEST_PAYLOAD_SIZE) return -1;
    field_out(r->transaction_id, sizeof r->transaction_id, p, 16);
    field_out(r->account_id, sizeof r->account_id, p + 16, 12);
    if (ib_unpack_decimal(p + 28, 6, &r->amount_cents)) return -1;
    field_out(r->currency, sizeof r->currency, p + 34, 3);
    field_out(r->operation, sizeof r->operation, p + 37, 5);
    return 0;
}

static int json_string(const char *json, const char *key, char *out, size_t cap) {
    char needle[64]; snprintf(needle, sizeof needle, "\"%s\"", key);
    const char *p = strstr(json, needle); if (!p) return -1;
    p = strchr(p + strlen(needle), ':'); if (!p) return -1; ++p;
    while (isspace((unsigned char)*p)) ++p;
    if (*p++ != '"') return -1;
    size_t n = 0; while (*p && *p != '"') { if (*p == '\\' || (unsigned char)*p < 32 || n + 1 >= cap) return -1; out[n++] = *p++; }
    if (*p != '"') return -1;
    out[n] = 0;
    return 0;
}

int ib_parse_json(const char *json, ib_request_t *r, char *error, size_t error_cap) {
    char amount[32];
    if (json_string(json, "transaction_id", r->transaction_id, sizeof r->transaction_id) ||
        json_string(json, "account_id", r->account_id, sizeof r->account_id) ||
        json_string(json, "amount", amount, sizeof amount) ||
        json_string(json, "currency", r->currency, sizeof r->currency) ||
        json_string(json, "operation", r->operation, sizeof r->operation)) goto bad;
    errno = 0; char *end = NULL; long double units = strtold(amount, &end);
    if (!end || *end || errno == ERANGE || !isfinite((double)units) || units < 0 || units > 999999999.99L) goto bad;
    r->amount_cents = (int64_t)(units * 100.0L + 0.5L);
    if (strlen(r->currency) != 3 || strlen(r->operation) < 4 || strlen(r->operation) > 5) goto bad;
    if (json_string(json, "correlation_id", r->correlation_id, sizeof r->correlation_id) != 0)
        snprintf(r->correlation_id, sizeof r->correlation_id, "%s", r->transaction_id);
    return 0;
bad:
    snprintf(error, error_cap, "JSON inválido ou incompatível com o copybook"); return -1;
}

int ib_encode_response(const ib_response_t *r, uint8_t *out, size_t cap, size_t *written) {
    if (cap < IB_RESPONSE_PAYLOAD_SIZE || strlen(r->status) != 2 || strlen(r->code) != 3 || strlen(r->transaction_id) > 16) return -1;
    memset(out, 0x40, IB_RESPONSE_PAYLOAD_SIZE);
    if (ib_ascii_to_ebcdic(r->status, out, 2) || ib_ascii_to_ebcdic(r->code, out + 2, 3) ||
        ib_ascii_to_ebcdic(r->message, out + 5, 64) || ib_ascii_to_ebcdic(r->transaction_id, out + 69, 16)) return -1;
    *written = IB_RESPONSE_PAYLOAD_SIZE; return 0;
}

int ib_decode_response(const uint8_t *p, size_t len, ib_response_t *r) {
    if (len != IB_RESPONSE_PAYLOAD_SIZE) return -1;
    field_out(r->status, sizeof r->status, p, 2); field_out(r->code, sizeof r->code, p + 2, 3);
    field_out(r->message, sizeof r->message, p + 5, 64); field_out(r->transaction_id, sizeof r->transaction_id, p + 69, 16); return 0;
}

int ib_response_json(const ib_response_t *r, char *out, size_t cap) {
    int n = snprintf(out, cap, "{\"status\":\"%s\",\"code\":\"%s\",\"message\":\"%s\",\"transaction_id\":\"%s\"}", r->status, r->code, r->message, r->transaction_id);
    return n < 0 || (size_t)n >= cap ? -1 : n;
}

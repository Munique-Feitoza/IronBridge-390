#ifndef IRONBRIDGE_H
#define IRONBRIDGE_H

#include <stdint.h>
#include <stddef.h>

#define IB_HEADER_SIZE 6u
#define IB_REQUEST_PAYLOAD_SIZE 42u
#define IB_RESPONSE_PAYLOAD_SIZE 85u
#define IB_MAX_FRAME 1024u
#define IB_MAX_FIELDS 32u

typedef enum { IB_FIELD_TEXT, IB_FIELD_DECIMAL } ib_field_kind_t;
typedef struct {
    char name[32];
    ib_field_kind_t kind;
    unsigned digits;
    unsigned scale;
    size_t offset;
    size_t size;
    int packed;
} ib_field_t;
typedef struct {
    ib_field_t fields[IB_MAX_FIELDS];
    size_t count;
    size_t record_size;
} ib_copybook_t;
typedef struct {
    char transaction_id[32];
    char account_id[32];
    char amount[32];
    char currency[32];
    char operation[32];
} ib_field_map_t;

typedef struct {
    char transaction_id[17];
    char account_id[13];
    int64_t amount_cents;
    char currency[4];
    char operation[6];
    char correlation_id[37];
} ib_request_t;

typedef struct {
    char status[3];
    char code[4];
    char message[65];
    char transaction_id[17];
} ib_response_t;

int ib_parse_json(const char *json, ib_request_t *out, char *error, size_t error_cap);
int ib_encode_request(const ib_request_t *request, uint8_t *out, size_t cap, size_t *written);
int ib_encode_request_schema(const ib_request_t *request, const ib_copybook_t *schema,
                             uint8_t *out, size_t cap, size_t *written);
int ib_encode_request_schema_map(const ib_request_t *request, const ib_copybook_t *schema,
                                 const ib_field_map_t *map, uint8_t *out, size_t cap, size_t *written);
int ib_decode_request(const uint8_t *payload, size_t len, ib_request_t *out);
int ib_encode_response(const ib_response_t *response, uint8_t *out, size_t cap, size_t *written);
int ib_decode_response(const uint8_t *payload, size_t len, ib_response_t *out);
int ib_response_json(const ib_response_t *response, char *out, size_t cap);

int ib_write_frame(int fd, uint8_t type, const uint8_t *payload, uint16_t length);
int ib_read_frame(int fd, uint8_t *type, uint8_t *payload, uint16_t *length, size_t cap);

int ib_copybook_load(const char *path, ib_copybook_t *schema, char *error, size_t error_cap);
void ib_copybook_mapping_default(ib_field_map_t *map);
int ib_copybook_mapping_load(const char *path, ib_field_map_t *map, char *error, size_t error_cap);

#endif

#ifndef EASY_MORNINGS_JSON_H
#define EASY_MORNINGS_JSON_H

#include <stdbool.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// String builder (used for writing JSON / response bodies)
// ---------------------------------------------------------------------------

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    bool overflow;
} sb_t;

void sb_init(sb_t *sb, char *buf, size_t cap);
void sb_append(sb_t *sb, const char *str);
void sb_printf(sb_t *sb, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
// Writes a number the way Python would: integers without a decimal point,
// other values with the shortest representation that round-trips.
void sb_json_number(sb_t *sb, float value);
void sb_json_bool(sb_t *sb, bool value);
void sb_json_string(sb_t *sb, const char *str);

// ---------------------------------------------------------------------------
// Minimal pull parser for JSON
// ---------------------------------------------------------------------------

typedef struct {
    const char *p;
    const char *end;
    const char *error;  // set on the first error
    int depth;
} json_parser_t;

void json_init(json_parser_t *jp, const char *data, size_t len);
bool json_fail(json_parser_t *jp, const char *error);

bool json_parse_string(json_parser_t *jp, char *out, size_t cap);
bool json_parse_number(json_parser_t *jp, float *out);
bool json_parse_bool(json_parser_t *jp, bool *out);
// If the next value is null, consumes it and returns true.
bool json_try_parse_null(json_parser_t *jp);
bool json_skip_value(json_parser_t *jp);
// Check nothing but whitespace remains.
bool json_expect_end(json_parser_t *jp);

// Iterating objects:
//   bool first = true, has_key;
//   if (!json_object_begin(jp)) ...
//   while (json_object_next(jp, &first, key, sizeof(key), &has_key) && has_key) {
//       ... parse value ...
//   }
//   if (jp->error) ...
bool json_object_begin(json_parser_t *jp);
bool json_object_next(json_parser_t *jp, bool *first, char *key, size_t key_cap, bool *has_key);

// Iterating arrays works the same way.
bool json_array_begin(json_parser_t *jp);
bool json_array_next(json_parser_t *jp, bool *first, bool *has_item);

#endif

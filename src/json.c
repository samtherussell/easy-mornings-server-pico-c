#include "json.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_MAX_DEPTH 16

// ---------------------------------------------------------------------------
// String builder
// ---------------------------------------------------------------------------

void sb_init(sb_t *sb, char *buf, size_t cap) {
    sb->buf = buf;
    sb->len = 0;
    sb->cap = cap;
    sb->overflow = false;
    if (cap > 0) {
        buf[0] = '\0';
    }
}

static void sb_write(sb_t *sb, const char *data, size_t n) {
    if (sb->overflow) {
        return;
    }
    if (sb->len + n + 1 > sb->cap) {
        sb->overflow = true;
        return;
    }
    memcpy(sb->buf + sb->len, data, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}

void sb_append(sb_t *sb, const char *str) {
    sb_write(sb, str, strlen(str));
}

void sb_printf(sb_t *sb, const char *fmt, ...) {
    if (sb->overflow) {
        return;
    }
    size_t space = sb->cap - sb->len;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(sb->buf + sb->len, space, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= space) {
        sb->buf[sb->len] = '\0';
        sb->overflow = true;
        return;
    }
    sb->len += (size_t)n;
}

void sb_json_number(sb_t *sb, float value) {
    if (!isfinite(value)) {
        sb_append(sb, "null");
        return;
    }
    if (value == floorf(value) && fabsf(value) < 1e9f) {
        sb_printf(sb, "%ld", (long)value);
        return;
    }
    char tmp[32];
    for (int precision = 1; precision <= 9; precision++) {
        snprintf(tmp, sizeof(tmp), "%.*g", precision, (double)value);
        if (strtof(tmp, NULL) == value) {
            break;
        }
    }
    sb_append(sb, tmp);
}

void sb_json_bool(sb_t *sb, bool value) {
    sb_append(sb, value ? "true" : "false");
}

void sb_json_string(sb_t *sb, const char *str) {
    sb_write(sb, "\"", 1);
    for (const char *c = str; *c; c++) {
        switch (*c) {
            case '"': sb_append(sb, "\\\""); break;
            case '\\': sb_append(sb, "\\\\"); break;
            case '\n': sb_append(sb, "\\n"); break;
            case '\r': sb_append(sb, "\\r"); break;
            case '\t': sb_append(sb, "\\t"); break;
            default:
                if ((unsigned char)*c < 0x20) {
                    sb_printf(sb, "\\u%04x", (unsigned char)*c);
                } else {
                    sb_write(sb, c, 1);
                }
        }
    }
    sb_write(sb, "\"", 1);
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

void json_init(json_parser_t *jp, const char *data, size_t len) {
    jp->p = data;
    jp->end = data + len;
    jp->error = NULL;
    jp->depth = 0;
}

bool json_fail(json_parser_t *jp, const char *error) {
    if (jp->error == NULL) {
        jp->error = error;
    }
    return false;
}

static void skip_ws(json_parser_t *jp) {
    while (jp->p < jp->end && (*jp->p == ' ' || *jp->p == '\t' || *jp->p == '\n' || *jp->p == '\r')) {
        jp->p++;
    }
}

static int peek(json_parser_t *jp) {
    skip_ws(jp);
    return jp->p < jp->end ? (unsigned char)*jp->p : -1;
}

static bool expect_char(json_parser_t *jp, char c) {
    if (peek(jp) != c) {
        return json_fail(jp, "invalid JSON");
    }
    jp->p++;
    return true;
}

static bool match_literal(json_parser_t *jp, const char *lit) {
    size_t n = strlen(lit);
    skip_ws(jp);
    if ((size_t)(jp->end - jp->p) >= n && memcmp(jp->p, lit, n) == 0) {
        jp->p += n;
        return true;
    }
    return false;
}

bool json_parse_string(json_parser_t *jp, char *out, size_t cap) {
    if (jp->error) return false;
    if (!expect_char(jp, '"')) {
        return json_fail(jp, "expected a string");
    }
    size_t n = 0;
    while (jp->p < jp->end) {
        char c = *jp->p++;
        if (c == '"') {
            if (out) out[n] = '\0';
            return true;
        }
        if (c == '\\') {
            if (jp->p >= jp->end) break;
            char e = *jp->p++;
            switch (e) {
                case '"': case '\\': case '/': c = e; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'u':
                    // non-ASCII is never needed here, just skip the code
                    if (jp->end - jp->p < 4) return json_fail(jp, "invalid JSON string escape");
                    jp->p += 4;
                    c = '?';
                    break;
                default:
                    return json_fail(jp, "invalid JSON string escape");
            }
        }
        if (out) {
            if (n + 1 >= cap) {
                return json_fail(jp, "JSON string too long");
            }
            out[n++] = c;
        }
    }
    return json_fail(jp, "unterminated JSON string");
}

bool json_parse_number(json_parser_t *jp, float *out) {
    if (jp->error) return false;
    skip_ws(jp);
    char tmp[32];
    size_t n = 0;
    while (jp->p + n < jp->end && n < sizeof(tmp) - 1 && strchr("+-0123456789.eE", jp->p[n])) {
        tmp[n] = jp->p[n];
        n++;
    }
    tmp[n] = '\0';
    char *endp;
    float value = strtof(tmp, &endp);
    if (n == 0 || endp != tmp + n || !isfinite(value)) {
        return json_fail(jp, "expected a number");
    }
    jp->p += n;
    *out = value;
    return true;
}

bool json_parse_bool(json_parser_t *jp, bool *out) {
    if (jp->error) return false;
    if (match_literal(jp, "true")) {
        *out = true;
        return true;
    }
    if (match_literal(jp, "false")) {
        *out = false;
        return true;
    }
    return json_fail(jp, "expected true or false");
}

bool json_try_parse_null(json_parser_t *jp) {
    if (jp->error) return false;
    return match_literal(jp, "null");
}

bool json_skip_value(json_parser_t *jp) {
    if (jp->error) return false;
    int c = peek(jp);
    bool first, more;
    float f;
    switch (c) {
        case '"':
            return json_parse_string(jp, NULL, 0);
        case '{': {
            if (!json_object_begin(jp)) return false;
            first = true;
            while (json_object_next(jp, &first, NULL, 0, &more) && more) {
                if (!json_skip_value(jp)) return false;
            }
            return jp->error == NULL;
        }
        case '[': {
            if (!json_array_begin(jp)) return false;
            first = true;
            while (json_array_next(jp, &first, &more) && more) {
                if (!json_skip_value(jp)) return false;
            }
            return jp->error == NULL;
        }
        case 't':
        case 'f': {
            bool b;
            return json_parse_bool(jp, &b);
        }
        case 'n':
            return json_try_parse_null(jp) || json_fail(jp, "invalid JSON");
        default:
            return json_parse_number(jp, &f);
    }
}

bool json_expect_end(json_parser_t *jp) {
    if (jp->error) return false;
    if (peek(jp) != -1) {
        return json_fail(jp, "unexpected data after JSON");
    }
    return true;
}

bool json_object_begin(json_parser_t *jp) {
    if (jp->error) return false;
    if (peek(jp) != '{') {
        return json_fail(jp, "expected a JSON object");
    }
    if (++jp->depth > JSON_MAX_DEPTH) {
        return json_fail(jp, "JSON nested too deeply");
    }
    jp->p++;
    return true;
}

bool json_object_next(json_parser_t *jp, bool *first, char *key, size_t key_cap, bool *has_key) {
    *has_key = false;
    if (jp->error) return false;
    int c = peek(jp);
    if (c == '}') {
        jp->p++;
        jp->depth--;
        return true;
    }
    if (!*first) {
        if (!expect_char(jp, ',')) return false;
    }
    *first = false;
    if (!json_parse_string(jp, key, key_cap)) return false;
    if (!expect_char(jp, ':')) return false;
    *has_key = true;
    return true;
}

bool json_array_begin(json_parser_t *jp) {
    if (jp->error) return false;
    if (peek(jp) != '[') {
        return json_fail(jp, "expected a JSON array");
    }
    if (++jp->depth > JSON_MAX_DEPTH) {
        return json_fail(jp, "JSON nested too deeply");
    }
    jp->p++;
    return true;
}

bool json_array_next(json_parser_t *jp, bool *first, bool *has_item) {
    *has_item = false;
    if (jp->error) return false;
    int c = peek(jp);
    if (c == ']') {
        jp->p++;
        jp->depth--;
        return true;
    }
    if (!*first) {
        if (!expect_char(jp, ',')) return false;
    }
    *first = false;
    if (peek(jp) == -1) {
        return json_fail(jp, "unterminated JSON array");
    }
    *has_item = true;
    return true;
}

#include "handler.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alarm.h"
#include "light.h"

#define MAX_ARGS 8

typedef struct {
    const char *key;
    const char *value;  // NULL if the arg had no '='
} arg_t;

typedef struct {
    char *path;
    arg_t args[MAX_ARGS];
    int arg_count;
} parsed_target_t;

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode(char *s) {
    char *out = s;
    for (char *in = s; *in; in++) {
        if (*in == '%' && hex_value(in[1]) >= 0 && hex_value(in[2]) >= 0) {
            *out++ = (char)(hex_value(in[1]) * 16 + hex_value(in[2]));
            in += 2;
        } else if (*in == '+') {
            *out++ = ' ';
        } else {
            *out++ = *in;
        }
    }
    *out = '\0';
}

static void parse_target(char *target, parsed_target_t *out) {
    char *hash = strchr(target, '#');
    if (hash) *hash = '\0';

    out->path = target;
    out->arg_count = 0;

    char *query = strchr(target, '?');
    if (query) {
        *query++ = '\0';
        char *save;
        for (char *arg = strtok_r(query, "&", &save); arg && out->arg_count < MAX_ARGS;
             arg = strtok_r(NULL, "&", &save)) {
            char *eq = strchr(arg, '=');
            if (eq) {
                *eq = '\0';
                url_decode(eq + 1);
            }
            url_decode(arg);
            out->args[out->arg_count++] = (arg_t){.key = arg, .value = eq ? eq + 1 : NULL};
        }
    }

    size_t len = strlen(out->path);
    if (len > 0 && out->path[len - 1] == '/') {
        out->path[len - 1] = '\0';
    }
}

static bool has_arg(const parsed_target_t *t, const char *key) {
    for (int i = 0; i < t->arg_count; i++) {
        if (strcmp(t->args[i].key, key) == 0) return true;
    }
    return false;
}

static const char *get_arg(const parsed_target_t *t, const char *key) {
    for (int i = 0; i < t->arg_count; i++) {
        if (strcmp(t->args[i].key, key) == 0) return t->args[i].value;
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// Responses
// ---------------------------------------------------------------------------

static void respond_json_success(http_response_t *resp) {
    resp->status = 200;
    resp->content_type = "application/json";
    sb_append(&resp->body, "{\"success\": true}");
}

static void respond_error(http_response_t *resp, const char *msg) {
    resp->status = 500;
    resp->content_type = NULL;
    resp->body.len = 0;
    resp->body.overflow = false;
    sb_append(&resp->body, msg);
}

static void respond_404(http_response_t *resp) {
    resp->status = 404;
    resp->content_type = NULL;
    sb_append(&resp->body, "Could not find resource.");
}

static bool get_float_arg(const parsed_target_t *t, const char *key, float min, float max, float *out,
                          http_response_t *resp) {
    char msg[64];
    const char *value = get_arg(t, key);
    if (value == NULL || *value == '\0') {
        snprintf(msg, sizeof(msg), "missing argument: %s", key);
        respond_error(resp, msg);
        return false;
    }
    char *end;
    float f = strtof(value, &end);
    if (*end != '\0' || !isfinite(f)) {
        snprintf(msg, sizeof(msg), "invalid number for %s: %s", key, value);
        respond_error(resp, msg);
        return false;
    }
    if (f < min || f > max) {
        snprintf(msg, sizeof(msg), "%s out of range: %s", key, value);
        respond_error(resp, msg);
        return false;
    }
    *out = f;
    return true;
}

static bool get_int_arg(const parsed_target_t *t, const char *key, int *out, http_response_t *resp) {
    char msg[64];
    const char *value = get_arg(t, key);
    char *end = NULL;
    long v = value ? strtol(value, &end, 10) : 0;
    if (value == NULL || *value == '\0' || *end != '\0') {
        snprintf(msg, sizeof(msg), "missing or invalid argument: %s", key);
        respond_error(resp, msg);
        return false;
    }
    *out = (int)v;
    return true;
}

// ---------------------------------------------------------------------------
// Endpoints
// ---------------------------------------------------------------------------

static void handle_light_program(const parsed_target_t *t, light_state_type_t type, http_response_t *resp) {
    light_program_t program = {.type = type};
    if (type != LIGHT_STATE_RAVE) {
        if (!get_float_arg(t, "level", 0.0f, 1.0f, &program.level, resp)) return;
    }
    if (type == LIGHT_STATE_FADING || type == LIGHT_STATE_TIMER) {
        if (!get_float_arg(t, "seconds", 0.0f, 7.0f * 24 * 3600, &program.seconds, resp)) return;
    }
    light_start(&program);
    respond_json_success(resp);
}

static void handle_alarm_post(const http_request_t *req, const parsed_target_t *t, http_response_t *resp) {
    char err[96];
    bool ok;
    if (has_arg(t, "index")) {
        int index;
        if (!get_int_arg(t, "index", &index, resp)) return;
        ok = alarms_update_json(index, req->body, req->body_len, err, sizeof(err));
    } else {
        ok = alarms_add_json(req->body, req->body_len, err, sizeof(err));
    }
    if (ok) {
        respond_json_success(resp);
    } else {
        respond_error(resp, err);
    }
}

static void handle_alarm_delete(const parsed_target_t *t, http_response_t *resp) {
    char err[96];
    int index;
    if (!get_int_arg(t, "index", &index, resp)) return;
    if (alarms_delete(index, err, sizeof(err))) {
        respond_json_success(resp);
    } else {
        respond_error(resp, err);
    }
}

void handler_handle(const http_request_t *req, http_response_t *resp) {
    parsed_target_t t;
    parse_target(req->target, &t);
    const char *path = t.path;

    if (strcmp(req->method, "GET") == 0) {
        if (strncmp(path, "/status", 7) == 0) {
            resp->status = 200;
            resp->content_type = "application/json";
            light_status_to_json(&resp->body);
            return;
        }
        if (strcmp(path, "/alarm") == 0) {
            resp->status = 200;
            resp->content_type = "application/json";
            alarms_to_json(&resp->body);
            if (resp->body.overflow) respond_error(resp, "response too large");
            return;
        }
    } else if (strcmp(req->method, "POST") == 0) {
        if (strcmp(path, "/now") == 0) return handle_light_program(&t, LIGHT_STATE_CONSTANT, resp);
        if (strcmp(path, "/fade") == 0) return handle_light_program(&t, LIGHT_STATE_FADING, resp);
        if (strcmp(path, "/timer") == 0) return handle_light_program(&t, LIGHT_STATE_TIMER, resp);
        if (strcmp(path, "/rave") == 0) return handle_light_program(&t, LIGHT_STATE_RAVE, resp);
        if (strcmp(path, "/alarm") == 0) return handle_alarm_post(req, &t, resp);
    } else if (strcmp(req->method, "DELETE") == 0) {
        if (strcmp(path, "/alarm") == 0) return handle_alarm_delete(&t, resp);
    }
    respond_404(resp);
}

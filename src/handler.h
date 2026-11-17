#ifndef EASY_MORNINGS_HANDLER_H
#define EASY_MORNINGS_HANDLER_H

#include <stddef.h>

#include "json.h"

typedef struct {
    const char *method;
    char *target;  // path + query string; may be modified by the handler
    const char *body;
    size_t body_len;
} http_request_t;

typedef struct {
    int status;
    const char *content_type;  // NULL for none
    sb_t body;
} http_response_t;

// Handle a request. resp->body is already initialised with a buffer.
void handler_handle(const http_request_t *req, http_response_t *resp);

#endif

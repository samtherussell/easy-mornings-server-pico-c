#include "http_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "lwip/tcp.h"

#include "handler.h"

#define MAX_CONNECTIONS 4
#define REQ_BUF_SIZE 2048
#define RESP_BODY_SIZE 4096
#define RESP_HEADER_SIZE 256
// tcp_poll is called every POLL_INTERVAL * 0.5s
#define POLL_INTERVAL 2
// close connections that are idle for this many polls
#define IDLE_TIMEOUT_POLLS 10

typedef enum {
    CONN_FREE,
    CONN_RECEIVING,
    CONN_SENDING,
} conn_state_t;

typedef struct {
    struct tcp_pcb *pcb;
    conn_state_t state;
    size_t len;
    size_t to_send;
    uint8_t idle_polls;
    char buf[REQ_BUF_SIZE + 1];
} conn_t;

static conn_t conns[MAX_CONNECTIONS];
// Requests are handled one at a time and tcp_write copies the data, so the
// response buffers can be shared.
static char resp_body[RESP_BODY_SIZE];
static char resp_header[RESP_HEADER_SIZE];

static const char *status_text(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default: return "Unknown";
    }
}

static err_t conn_close(conn_t *c) {
    err_t ret = ERR_OK;
    if (c->pcb) {
        tcp_arg(c->pcb, NULL);
        tcp_recv(c->pcb, NULL);
        tcp_sent(c->pcb, NULL);
        tcp_err(c->pcb, NULL);
        tcp_poll(c->pcb, NULL, 0);
        if (tcp_close(c->pcb) != ERR_OK) {
            tcp_abort(c->pcb);
            ret = ERR_ABRT;
        }
        c->pcb = NULL;
    }
    c->state = CONN_FREE;
    return ret;
}

static err_t send_response(conn_t *c, int status, const char *content_type, const char *body, size_t body_len) {
    int n = snprintf(resp_header, sizeof(resp_header),
                     "HTTP/1.1 %d %s\r\n"
                     "%s%s%s"
                     "Content-Length: %u\r\n"
                     "Connection: close\r\n"
                     "\r\n",
                     status, status_text(status),
                     content_type ? "Content-Type: " : "", content_type ? content_type : "",
                     content_type ? "\r\n" : "", (unsigned)body_len);
    size_t total = (size_t)n + body_len;

    if (tcp_sndbuf(c->pcb) < total) {
        printf("http: response too large for send buffer\n");
        return conn_close(c);
    }
    err_t err = tcp_write(c->pcb, resp_header, (u16_t)n, TCP_WRITE_FLAG_COPY | (body_len ? TCP_WRITE_FLAG_MORE : 0));
    if (err == ERR_OK && body_len > 0) {
        err = tcp_write(c->pcb, body, (u16_t)body_len, TCP_WRITE_FLAG_COPY);
    }
    if (err != ERR_OK) {
        printf("http: tcp_write failed: %d\n", err);
        return conn_close(c);
    }
    c->state = CONN_SENDING;
    c->to_send = total;
    tcp_output(c->pcb);
    return ERR_OK;
}

static err_t send_error(conn_t *c, int status, const char *msg) {
    printf("http: %d %s\n", status, msg);
    return send_response(c, status, NULL, msg, strlen(msg));
}

// Find the value of Content-Length in the headers (between start and end).
static bool get_content_length(const char *start, const char *end, size_t *out) {
    *out = 0;
    const char *line = start;
    while (line < end) {
        const char *eol = strstr(line, "\r\n");
        if (eol == NULL || eol > end) eol = end;
        static const char name[] = "content-length:";
        if ((size_t)(eol - line) > sizeof(name) - 1 && strncasecmp(line, name, sizeof(name) - 1) == 0) {
            char *num_end;
            unsigned long v = strtoul(line + sizeof(name) - 1, &num_end, 10);
            while (num_end < eol && (*num_end == ' ' || *num_end == '\t')) num_end++;
            if (num_end != eol) return false;
            *out = v;
            return true;
        }
        line = eol + 2;
    }
    return true;
}

static err_t process_request(conn_t *c, bool overflow) {
    char *header_end = strstr(c->buf, "\r\n\r\n");
    if (header_end == NULL) {
        if (overflow || c->len >= REQ_BUF_SIZE) {
            return send_error(c, 413, "Request too large");
        }
        return ERR_OK;  // wait for more data
    }

    char *first_line_end = strstr(c->buf, "\r\n");
    size_t header_len = (size_t)(header_end - c->buf) + 4;
    size_t content_length;
    if (!get_content_length(first_line_end + 2, header_end, &content_length)) {
        return send_error(c, 400, "bad Content-Length header");
    }
    if (header_len + content_length > REQ_BUF_SIZE) {
        return send_error(c, 413, "Request too large");
    }
    if (c->len < header_len + content_length) {
        return ERR_OK;  // wait for the rest of the body
    }

    // request line: METHOD TARGET VERSION
    *first_line_end = '\0';
    char *save;
    char *method = strtok_r(c->buf, " ", &save);
    char *target = strtok_r(NULL, " ", &save);
    char *version = strtok_r(NULL, " ", &save);
    if (method == NULL || target == NULL || version == NULL || strtok_r(NULL, " ", &save) != NULL) {
        return send_error(c, 400, "bad http request first line");
    }
    if (strcmp(version, "HTTP/1.1") != 0 && strcmp(version, "HTTP/1.0") != 0) {
        return send_error(c, 400, "unsupported http request version");
    }
    if (strcmp(method, "GET") != 0 && strcmp(method, "POST") != 0 && strcmp(method, "DELETE") != 0) {
        return send_error(c, 501, "unsupported http request method");
    }

    printf("http: %s %s", method, target);

    http_request_t req = {
        .method = method,
        .target = target,
        .body = c->buf + header_len,
        .body_len = content_length,
    };
    http_response_t resp = {.status = 500, .content_type = NULL};
    sb_init(&resp.body, resp_body, sizeof(resp_body));

    handler_handle(&req, &resp);

    if (resp.body.overflow) {
        printf("\n");
        return send_error(c, 500, "response too large");
    }
    printf(" -> %d\n", resp.status);
    return send_response(c, resp.status, resp.content_type, resp.body.buf, resp.body.len);
}

// ---------------------------------------------------------------------------
// lwIP callbacks
// ---------------------------------------------------------------------------

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    conn_t *c = arg;
    if (c == NULL) {
        if (p) pbuf_free(p);
        tcp_abort(pcb);
        return ERR_ABRT;
    }
    if (p == NULL) {
        // the client closed the connection
        return conn_close(c);
    }
    tcp_recved(pcb, p->tot_len);
    c->idle_polls = 0;

    if (c->state != CONN_RECEIVING || err != ERR_OK) {
        pbuf_free(p);
        return ERR_OK;
    }

    size_t space = REQ_BUF_SIZE - c->len;
    size_t n = p->tot_len < space ? p->tot_len : space;
    bool overflow = p->tot_len > space;
    pbuf_copy_partial(p, c->buf + c->len, (u16_t)n, 0);
    c->len += n;
    c->buf[c->len] = '\0';
    pbuf_free(p);

    return process_request(c, overflow);
}

static err_t on_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    (void)pcb;
    conn_t *c = arg;
    if (c == NULL) return ERR_OK;
    c->idle_polls = 0;
    if (len >= c->to_send) {
        c->to_send = 0;
        return conn_close(c);
    }
    c->to_send -= len;
    return ERR_OK;
}

static err_t on_poll(void *arg, struct tcp_pcb *pcb) {
    conn_t *c = arg;
    if (c == NULL) {
        tcp_abort(pcb);
        return ERR_ABRT;
    }
    if (++c->idle_polls >= IDLE_TIMEOUT_POLLS) {
        printf("http: connection timed out\n");
        return conn_close(c);
    }
    return ERR_OK;
}

static void on_err(void *arg, err_t err) {
    conn_t *c = arg;
    if (c == NULL) return;
    if (err != ERR_ABRT) {
        printf("http: connection error %d\n", err);
    }
    // lwIP has already freed the pcb
    c->pcb = NULL;
    c->state = CONN_FREE;
}

static err_t on_accept(void *arg, struct tcp_pcb *pcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || pcb == NULL) {
        return ERR_VAL;
    }
    conn_t *c = NULL;
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (conns[i].state == CONN_FREE) {
            c = &conns[i];
            break;
        }
    }
    if (c == NULL) {
        printf("http: too many connections\n");
        tcp_abort(pcb);
        return ERR_ABRT;
    }
    c->pcb = pcb;
    c->state = CONN_RECEIVING;
    c->len = 0;
    c->buf[0] = '\0';
    c->to_send = 0;
    c->idle_polls = 0;

    tcp_arg(pcb, c);
    tcp_recv(pcb, on_recv);
    tcp_sent(pcb, on_sent);
    tcp_err(pcb, on_err);
    tcp_poll(pcb, on_poll, POLL_INTERVAL);
    return ERR_OK;
}

bool http_server_start(uint16_t port) {
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb == NULL) {
        printf("http: failed to create pcb\n");
        return false;
    }
    if (tcp_bind(pcb, NULL, port) != ERR_OK) {
        printf("http: failed to bind to port %u\n", port);
        tcp_close(pcb);
        return false;
    }
    struct tcp_pcb *listen_pcb = tcp_listen_with_backlog(pcb, MAX_CONNECTIONS);
    if (listen_pcb == NULL) {
        printf("http: failed to listen\n");
        tcp_close(pcb);
        return false;
    }
    tcp_accept(listen_pcb, on_accept);
    printf("http: listening on port %u\n", port);
    return true;
}

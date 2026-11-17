#ifndef EASY_MORNINGS_HTTP_SERVER_H
#define EASY_MORNINGS_HTTP_SERVER_H

#include <stdbool.h>
#include <stdint.h>

// Start listening for HTTP requests. Each connection handles one request and
// is then closed (like the MicroPython version).
bool http_server_start(uint16_t port);

#endif

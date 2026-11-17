#ifndef EASY_MORNINGS_WIFI_H
#define EASY_MORNINGS_WIFI_H

#include <stdbool.h>

// Start connecting to WiFi. Connection happens in the background; call
// wifi_poll() regularly. The onboard LED flashes while connecting, is on when
// connected, and off while waiting to retry after a failure.
void wifi_start(void);

// Keep the connection alive, reconnecting if it drops. Call regularly.
void wifi_poll(void);

bool wifi_is_connected(void);

#endif

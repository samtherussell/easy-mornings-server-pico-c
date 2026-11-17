#include "wifi.h"

#include <stdio.h>
#include <string.h>

#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "pico/cyw43_arch.h"

#include "clock.h"
#include "config.h"

#define LED_FLASH_MS 500

typedef enum {
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_WAIT_RETRY,
} wifi_state_t;

static wifi_state_t state;
static absolute_time_t state_since;
static absolute_time_t next_led_toggle;
static bool led_on;

static void set_led(bool on) {
    led_on = on;
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

static void set_state(wifi_state_t new_state) {
    state = new_state;
    state_since = get_absolute_time();
}

static void begin_connect(void) {
    const char *password = strlen(WIFI_PASSWORD) > 0 ? WIFI_PASSWORD : NULL;
    uint32_t auth = password ? CYW43_AUTH_WPA2_AES_PSK : CYW43_AUTH_OPEN;
    printf("wifi: connecting to \"%s\"\n", WIFI_SSID);
    int err = cyw43_arch_wifi_connect_async(WIFI_SSID, password, auth);
    if (err) {
        printf("wifi: failed to start connecting (%d)\n", err);
        set_led(false);
        set_state(WIFI_WAIT_RETRY);
        return;
    }
    set_state(WIFI_CONNECTING);
}

static void give_up_and_retry(const char *reason) {
    printf("wifi: %s, retrying in %d s\n", reason, WIFI_RETRY_DELAY_MS / 1000);
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    set_led(false);
    set_state(WIFI_WAIT_RETRY);
}

void wifi_start(void) {
    if (strlen(WIFI_SSID) == 0) {
        printf("wifi: WARNING no SSID set, see config.h\n");
    }
    cyw43_arch_enable_sta_mode();
    begin_connect();
}

static uint32_t elapsed_ms(void) {
    return (uint32_t)(absolute_time_diff_us(state_since, get_absolute_time()) / 1000);
}

void wifi_poll(void) {
    int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    switch (state) {
        case WIFI_CONNECTING:
            if (status == CYW43_LINK_UP) {
                struct netif *netif = &cyw43_state.netif[CYW43_ITF_STA];
                printf("wifi: connected, ip: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
                // turn power saving off, it makes the server slow to respond
                cyw43_wifi_pm(&cyw43_state, CYW43_NONE_PM);
                set_led(true);
                set_state(WIFI_CONNECTED);
                clock_start_sntp();
            } else if (status == CYW43_LINK_BADAUTH) {
                give_up_and_retry("wrong password");
            } else if (status == CYW43_LINK_NONET) {
                give_up_and_retry("network not found");
            } else if (status == CYW43_LINK_FAIL) {
                give_up_and_retry("connection failed");
            } else if (elapsed_ms() > WIFI_CONNECT_TIMEOUT_MS) {
                give_up_and_retry("timed out");
            } else if (time_reached(next_led_toggle)) {
                set_led(!led_on);
                next_led_toggle = make_timeout_time_ms(LED_FLASH_MS);
            }
            break;

        case WIFI_CONNECTED:
            if (status != CYW43_LINK_UP) {
                give_up_and_retry("connection lost");
            }
            break;

        case WIFI_WAIT_RETRY:
            if (elapsed_ms() > WIFI_RETRY_DELAY_MS) {
                begin_connect();
            }
            break;
    }
}

bool wifi_is_connected(void) {
    return state == WIFI_CONNECTED;
}

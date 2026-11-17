#ifndef EASY_MORNINGS_CONFIG_H
#define EASY_MORNINGS_CONFIG_H

// ---------------------------------------------------------------------------
// WiFi credentials.
// Either edit the defaults below, or pass them at configure time:
//   cmake -DWIFI_SSID="my-network" -DWIFI_PASSWORD="secret" ..
// (or set the WIFI_SSID / WIFI_PASSWORD environment variables before
// configuring). An empty password connects to an open network.
// ---------------------------------------------------------------------------
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// ---------------------------------------------------------------------------
// Local timezone used for alarms, as a POSIX TZ string.
// Default is UK time (GMT, BST in summer). Other examples:
//   "CET-1CEST,M3.5.0,M10.5.0/3"   central Europe
//   "EST5EDT,M3.2.0,M11.1.0"       US eastern
//   "UTC0"                         UTC
// See https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
// ---------------------------------------------------------------------------
#ifndef TIMEZONE
#define TIMEZONE "GMT0BST,M3.5.0/1,M10.5.0"
#endif

// GPIO driving the light (same as the MicroPython version)
#define LIGHT_PIN 28
// PWM frequency for the light. The RP2040 can only go this high with a full
// 16-bit duty range if sys clock >= 131 MHz; below that it runs a bit slower.
#define PWM_FREQ_HZ 2000

#define HTTP_PORT 8080

// How often the light level is updated
#define LIGHT_TICK_MS 20
// How often alarms are checked
#define ALARM_CHECK_MS 1000
#define MAX_ALARMS 16

#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_RETRY_DELAY_MS 5000

// The RP2040 watchdog maximum is ~8.3s
#define WATCHDOG_TIMEOUT_MS 8000

#endif

// Easy Mornings server for the Raspberry Pi Pico W.
//
// Controls a light on GPIO 28 over HTTP (port 8080), with fades, timers and
// alarms. Everything runs from a single loop: lwIP is used in poll mode, so
// network callbacks run inside cyw43_arch_poll() and never race the light logic.

#include <stdio.h>

#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "config.h"
#include "src/alarm.h"
#include "src/clock.h"
#include "src/http_server.h"
#include "src/light.h"
#include "src/wifi.h"

int main(void) {
    stdio_init_all();

    // get the light into a known (off) state as early as possible
    light_init();
    clock_init();

    printf("easy mornings server starting\n");
    if (watchdog_caused_reboot()) {
        printf("note: restarted by watchdog\n");
    }

    if (cyw43_arch_init()) {
        printf("failed to initialise wifi chip, rebooting\n");
        sleep_ms(5000);
        watchdog_reboot(0, 0, 0);
    }

    wifi_start();
    if (!http_server_start(HTTP_PORT)) {
        printf("failed to start http server, rebooting\n");
        sleep_ms(5000);
        watchdog_reboot(0, 0, 0);
    }

    // reboot automatically if the main loop ever stops running
    watchdog_enable(WATCHDOG_TIMEOUT_MS, true);

    absolute_time_t next_light_tick = get_absolute_time();
    absolute_time_t next_alarm_check = get_absolute_time();

    while (true) {
        // handle wifi / network work (runs lwIP callbacks)
        cyw43_arch_poll();

        if (time_reached(next_light_tick)) {
            light_tick();
            wifi_poll();
            next_light_tick = delayed_by_ms(next_light_tick, LIGHT_TICK_MS);
            if (time_reached(next_light_tick)) {
                // fell behind, don't try to catch up
                next_light_tick = make_timeout_time_ms(LIGHT_TICK_MS);
            }
        }

        if (time_reached(next_alarm_check)) {
            alarms_check();
            next_alarm_check = make_timeout_time_ms(ALARM_CHECK_MS);
        }

        watchdog_update();

        // sleep until there is network work to do or the next tick is due
        cyw43_arch_wait_for_work_until(next_light_tick);
    }
}

# Easy Mornings Server (Pico W, C)

A C port of `easy-mornings-pico-server` (MicroPython) for the Raspberry Pi Pico W.
It serves the same HTTP API on port 8080, so the Android app works unchanged.

## Setup

1. Set your WiFi credentials, either by:
   - editing `WIFI_SSID` / `WIFI_PASSWORD` in `config.h`, or
   - passing `-DWIFI_SSID=... -DWIFI_PASSWORD=...` to cmake, or setting them as
     environment variables before configuring.
2. Check `TIMEZONE` in `config.h` (default: UK time). Alarms use local time.
3. Build with the Raspberry Pi Pico VS Code extension (Compile), or:
   ```
   cmake -S . -B build -G Ninja
   ninja -C build
   ```
4. Hold BOOTSEL, plug in the Pico W and copy `build/easy-mornings-server-pico-c.uf2` onto it.

Debug output (IP address, requests, alarms) is printed over USB serial.

## Behaviour

- **Light:** GPIO 28, using PWM for levels between 0 and 1 (the duty cycle is the
  square of the level, so the brightness looks roughly linear).
- **Onboard LED:** flashes while connecting to WiFi, stays on once connected, and
  goes off while waiting to retry after a failure. WiFi reconnects automatically
  if the connection drops.
- **Clock:** set over NTP (`pool.ntp.org`) and re-synced every hour. Alarms don't
  fire until the clock has been set.
- **Watchdog:** the Pico reboots itself if the main loop ever hangs.
- **Alarms are only stored in memory,** so they're lost on power loss or reboot
  (as in the MicroPython version).

## API

| Method | Path | Args / body | Response |
|---|---|---|---|
| GET | `/status` | | `{"level": 0.5, "state": "LIGHT_STATE_FADING", "time_left": 42}` |
| POST | `/now` | `level` (0-1) | `{"success": true}` |
| POST | `/fade` | `level`, `seconds` | `{"success": true}` |
| POST | `/timer` | `level`, `seconds` | `{"success": true}` |
| POST | `/rave` | | `{"success": true}` |
| GET | `/alarm` | | list of alarms |
| POST | `/alarm` | JSON alarm body | adds an alarm |
| POST | `/alarm?index=N` | JSON (partial) alarm body | updates alarm N |
| DELETE | `/alarm?index=N` | | deletes alarm N |

Alarm JSON:
```json
{
  "time": [7, 30],
  "state": {"type": "LIGHT_STATE_FADING", "level": 1, "seconds": 900},
  "repeat": true,
  "days": [true, true, true, true, true, false, false]
}
```
`state.type` is one of `LIGHT_STATE_CONSTANT` (needs `level`), `LIGHT_STATE_FADING`
and `LIGHT_STATE_TIMER` (need `level` and `seconds`), or `LIGHT_STATE_RAVE`.
`days` starts on Monday, or is `null` for every day. `repeat` defaults to `false`,
and a non-repeating alarm disables itself after it goes off.

Errors return status 500 with a plain-text message; unknown paths return 404.

## Layout

- `easy-mornings-server-pico-c.c`: `main()` and the main loop
- `config.h`: WiFi, timezone, pin, port and timing settings
- `lwipopts.h`: lwIP network stack and SNTP settings
- `src/light.c`: light controller and light states (constant, fade, timer, rave)
- `src/alarm.c`: alarms
- `src/http_server.c`: TCP/HTTP server on lwIP's raw API
- `src/handler.c`: request routing
- `src/json.c`: small JSON reader/writer
- `src/wifi.c`: WiFi connection and reconnection
- `src/clock.c`: NTP time and timezone

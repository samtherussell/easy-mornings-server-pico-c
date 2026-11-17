#include "clock.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "lwip/apps/sntp.h"

#include "config.h"

static bool clock_set = false;
static bool sntp_started = false;

void clock_init(void) {
    setenv("TZ", TIMEZONE, 1);
    tzset();
}

void clock_start_sntp(void) {
    if (sntp_started) {
        return;
    }
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_init();
    sntp_started = true;
}

bool clock_is_set(void) {
    return clock_set;
}

bool clock_get_local(struct tm *out) {
    if (!clock_set) {
        return false;
    }
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t t = tv.tv_sec;
    localtime_r(&t, out);
    return true;
}

// Called by lwIP SNTP (see lwipopts.h) when a valid time is received
void clock_set_utc(uint32_t sec, uint32_t us) {
    struct timeval tv = {.tv_sec = (time_t)sec, .tv_usec = (suseconds_t)us};
    settimeofday(&tv, NULL);
    bool first = !clock_set;
    clock_set = true;

    struct tm tm;
    clock_get_local(&tm);
    printf("clock %s: %04d-%02d-%02d %02d:%02d:%02d\n", first ? "set" : "updated",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

// Called by lwIP SNTP (see lwipopts.h) to read the current time
void clock_get_utc(uint32_t *sec, uint32_t *us) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *sec = (uint32_t)tv.tv_sec;
    *us = (uint32_t)tv.tv_usec;
}

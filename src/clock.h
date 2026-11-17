#ifndef EASY_MORNINGS_CLOCK_H
#define EASY_MORNINGS_CLOCK_H

#include <stdbool.h>
#include <time.h>

// Set up the local timezone. Call once at startup.
void clock_init(void);

// Start syncing the clock over NTP. Call once the network is up.
void clock_start_sntp(void);

// True once the clock has been set from NTP.
bool clock_is_set(void);

// Get the current local time. Returns false if the clock has not been set yet.
bool clock_get_local(struct tm *out);

#endif

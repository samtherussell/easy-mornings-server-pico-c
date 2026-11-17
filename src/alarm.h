#ifndef EASY_MORNINGS_ALARM_H
#define EASY_MORNINGS_ALARM_H

#include <stdbool.h>
#include <stddef.h>

#include "json.h"

// All functions that can fail return false and write a message into err.

// Add an alarm from JSON:
//   {"time": [7, 30], "state": {"type": "LIGHT_STATE_FADING", "level": 1, "seconds": 600},
//    "repeat": true, "days": [true, true, true, true, true, false, false], "enabled": true}
// "repeat", "days" (Monday first, or null for every day) and "enabled" are optional.
bool alarms_add_json(const char *json, size_t len, char *err, size_t err_cap);

// Update an alarm: fields present in the JSON replace the existing values.
bool alarms_update_json(int index, const char *json, size_t len, char *err, size_t err_cap);

bool alarms_delete(int index, char *err, size_t err_cap);

// Writes a JSON list of all alarms.
void alarms_to_json(sb_t *sb);

// Start the light program of any alarm that is due. Call regularly.
void alarms_check(void);

#endif

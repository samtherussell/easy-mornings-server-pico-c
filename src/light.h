#ifndef EASY_MORNINGS_LIGHT_H
#define EASY_MORNINGS_LIGHT_H

#include <stdbool.h>

#include "json.h"

typedef enum {
    LIGHT_STATE_CONSTANT,
    LIGHT_STATE_FADING,
    LIGHT_STATE_TIMER,
    LIGHT_STATE_RAVE,
} light_state_type_t;

// A description of something for the light to do, e.g. "fade to 0.8 over
// 600 seconds". Equivalent to the LightStateFactory classes in the
// MicroPython version: it is turned into a running state when started.
typedef struct {
    light_state_type_t type;
    float level;    // target level (0-1), unused for rave
    float seconds;  // duration, only used for fading and timer
} light_program_t;

void light_init(void);

// Start a program, relative to the current time and light level.
void light_start(const light_program_t *program);

// Update the light output. Call regularly (every LIGHT_TICK_MS).
void light_tick(void);

// Writes {"level": ..., "state": ..., "time_left": ...}
void light_status_to_json(sb_t *sb);

const char *light_state_name(light_state_type_t type);
bool light_state_from_name(const char *name, light_state_type_t *out);

// Writes the program as {"type": ..., "level": ..., "seconds": ...}
void light_program_to_json(const light_program_t *program, sb_t *sb);

#endif

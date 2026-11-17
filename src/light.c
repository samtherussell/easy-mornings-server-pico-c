#include "light.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/time.h"

#include "config.h"

#define PWM_WRAP 65535u
#define RAVE_WAVELENGTH_MS 250

typedef struct {
    light_state_type_t type;
    float start_level;
    float end_level;  // for constant: the level
    absolute_time_t start_time;
    absolute_time_t end_time;
} light_state_t;

static light_state_t state;
static float current_level;
static bool pwm_active;
static uint pwm_slice;
static uint pwm_channel;

static const char *const state_names[] = {
    [LIGHT_STATE_CONSTANT] = "LIGHT_STATE_CONSTANT",
    [LIGHT_STATE_FADING] = "LIGHT_STATE_FADING",
    [LIGHT_STATE_TIMER] = "LIGHT_STATE_TIMER",
    [LIGHT_STATE_RAVE] = "LIGHT_STATE_RAVE",
};

const char *light_state_name(light_state_type_t type) {
    return state_names[type];
}

bool light_state_from_name(const char *name, light_state_type_t *out) {
    for (size_t i = 0; i < sizeof(state_names) / sizeof(state_names[0]); i++) {
        if (strcmp(name, state_names[i]) == 0) {
            *out = (light_state_type_t)i;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Light controller: drives the pin
// ---------------------------------------------------------------------------

static void controller_set_level(float level) {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    if (level == current_level) {
        return;
    }
    current_level = level;

    if (level == 0.0f || level == 1.0f) {
        // fully on/off: drive the pin directly
        gpio_put(LIGHT_PIN, level == 1.0f);
        if (pwm_active) {
            gpio_set_function(LIGHT_PIN, GPIO_FUNC_SIO);
            pwm_active = false;
        }
    } else {
        // squared so that brightness looks roughly linear to the eye
        uint32_t duty = 2 + (uint32_t)lroundf(level * level * (float)(PWM_WRAP - 2));
        if (duty > PWM_WRAP) duty = PWM_WRAP;
        pwm_set_chan_level(pwm_slice, pwm_channel, (uint16_t)duty);
        if (!pwm_active) {
            gpio_set_function(LIGHT_PIN, GPIO_FUNC_PWM);
            pwm_active = true;
        }
    }
}

void light_init(void) {
    gpio_init(LIGHT_PIN);
    gpio_set_dir(LIGHT_PIN, GPIO_OUT);
    gpio_put(LIGHT_PIN, 0);

    pwm_slice = pwm_gpio_to_slice_num(LIGHT_PIN);
    pwm_channel = pwm_gpio_to_channel(LIGHT_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    float div = (float)clock_get_hz(clk_sys) / ((float)PWM_FREQ_HZ * (float)(PWM_WRAP + 1));
    if (div < 1.0f) div = 1.0f;
    pwm_config_set_clkdiv(&cfg, div);
    pwm_init(pwm_slice, &cfg, true);
    pwm_set_chan_level(pwm_slice, pwm_channel, 0);

    current_level = 0.0f;
    pwm_active = false;
    state = (light_state_t){.type = LIGHT_STATE_CONSTANT, .end_level = 0.0f};
}

// ---------------------------------------------------------------------------
// Light states
// ---------------------------------------------------------------------------

static int64_t floor_div(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

static int seconds_left(absolute_time_t now) {
    switch (state.type) {
        case LIGHT_STATE_FADING:
        case LIGHT_STATE_TIMER:
            return (int)floor_div(absolute_time_diff_us(now, state.end_time), 1000000);
        default:
            return -1;
    }
}

static bool is_finished(absolute_time_t now) {
    switch (state.type) {
        case LIGHT_STATE_FADING:
        case LIGHT_STATE_TIMER:
            return absolute_time_diff_us(now, state.end_time) < 0;
        default:
            return false;
    }
}

static float state_level(absolute_time_t now) {
    switch (state.type) {
        case LIGHT_STATE_CONSTANT:
            return state.end_level;
        case LIGHT_STATE_FADING: {
            int64_t total = absolute_time_diff_us(state.start_time, state.end_time);
            float progress;
            if (total <= 0) {
                progress = 1.0f;
            } else {
                progress = (float)absolute_time_diff_us(state.start_time, now) / (float)total;
                if (progress < 0.0f) progress = 0.0f;
                if (progress > 1.0f) progress = 1.0f;
            }
            return state.start_level + progress * (state.end_level - state.start_level);
        }
        case LIGHT_STATE_TIMER:
            return is_finished(now) ? state.end_level : state.start_level;
        case LIGHT_STATE_RAVE: {
            uint32_t ms = to_ms_since_boot(now);
            return (ms % RAVE_WAVELENGTH_MS) < (RAVE_WAVELENGTH_MS / 2) ? 1.0f : 0.0f;
        }
    }
    return 0.0f;
}

void light_start(const light_program_t *program) {
    absolute_time_t now = get_absolute_time();
    int64_t duration_us = (int64_t)((double)program->seconds * 1e6);

    state.type = program->type;
    state.start_time = now;
    state.start_level = current_level;
    state.end_level = program->level;
    state.end_time = delayed_by_us(now, (uint64_t)(duration_us > 0 ? duration_us : 0));

    printf("light: %s level=%.3f seconds=%.1f\n", light_state_name(program->type),
           (double)program->level, (double)program->seconds);

    // apply straight away rather than waiting for the next tick
    light_tick();
}

void light_tick(void) {
    absolute_time_t now = get_absolute_time();
    float level = state_level(now);
    controller_set_level(level);
    if (is_finished(now)) {
        printf("light: finished, level=%.3f\n", (double)level);
        state = (light_state_t){.type = LIGHT_STATE_CONSTANT, .end_level = level};
    }
}

void light_status_to_json(sb_t *sb) {
    absolute_time_t now = get_absolute_time();
    sb_append(sb, "{\"level\": ");
    sb_json_number(sb, current_level);
    sb_append(sb, ", \"state\": ");
    sb_json_string(sb, light_state_name(state.type));
    sb_printf(sb, ", \"time_left\": %d}", seconds_left(now));
}

void light_program_to_json(const light_program_t *program, sb_t *sb) {
    sb_append(sb, "{\"type\": ");
    sb_json_string(sb, light_state_name(program->type));
    if (program->type != LIGHT_STATE_RAVE) {
        sb_append(sb, ", \"level\": ");
        sb_json_number(sb, program->level);
    }
    if (program->type == LIGHT_STATE_FADING || program->type == LIGHT_STATE_TIMER) {
        sb_append(sb, ", \"seconds\": ");
        sb_json_number(sb, program->seconds);
    }
    sb_append(sb, "}");
}

#include "alarm.h"

#include <stdio.h>
#include <string.h>

#include "clock.h"
#include "config.h"
#include "light.h"

typedef struct {
    bool enabled;
    int hour;
    int minute;
    light_program_t program;
    bool repeat;
    bool has_days;
    bool days[7];  // Monday first

    // Date (yyyymmdd) this alarm last went off. It is worked out when the
    // alarm is created: if the time has already passed today, it counts as
    // having gone off today. Deferred until the clock is set.
    bool needs_init;
    bool has_last_triggered;
    int last_triggered;
} alarm_t;

static alarm_t alarms[MAX_ALARMS];
static int alarm_count = 0;

static int date_number(const struct tm *tm) {
    return (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday;
}

static void init_last_triggered(alarm_t *alarm, const struct tm *now) {
    bool time_passed = now->tm_hour > alarm->hour ||
                       (now->tm_hour == alarm->hour && now->tm_min >= alarm->minute);
    alarm->has_last_triggered = time_passed;
    alarm->last_triggered = date_number(now);
    alarm->needs_init = false;
}

static void reset_last_triggered(alarm_t *alarm) {
    struct tm now;
    if (clock_get_local(&now)) {
        init_last_triggered(alarm, &now);
    } else {
        alarm->needs_init = true;
    }
}

static bool alarm_triggered(alarm_t *alarm, const struct tm *now) {
    if (!alarm->enabled) {
        return false;
    }
    if (now->tm_hour < alarm->hour || (now->tm_hour == alarm->hour && now->tm_min < alarm->minute)) {
        return false;
    }
    int weekday = (now->tm_wday + 6) % 7;  // tm_wday is Sunday first
    if (alarm->has_days && !alarm->days[weekday]) {
        return false;
    }
    int today = date_number(now);
    if (alarm->has_last_triggered && alarm->last_triggered >= today) {
        return false;
    }
    alarm->has_last_triggered = true;
    alarm->last_triggered = today;
    if (!alarm->repeat) {
        alarm->enabled = false;
    }
    return true;
}

void alarms_check(void) {
    struct tm now;
    if (!clock_get_local(&now)) {
        return;
    }
    for (int i = 0; i < alarm_count; i++) {
        alarm_t *alarm = &alarms[i];
        if (alarm->needs_init) {
            init_last_triggered(alarm, &now);
        }
        if (alarm_triggered(alarm, &now)) {
            printf("alarm %d triggered\n", i);
            light_start(&alarm->program);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------

static void alarm_to_json(const alarm_t *alarm, sb_t *sb) {
    sb_printf(sb, "{\"time\": [%d, %d], \"state\": ", alarm->hour, alarm->minute);
    light_program_to_json(&alarm->program, sb);
    sb_append(sb, ", \"repeat\": ");
    sb_json_bool(sb, alarm->repeat);
    sb_append(sb, ", \"days\": ");
    if (alarm->has_days) {
        sb_append(sb, "[");
        for (int d = 0; d < 7; d++) {
            if (d > 0) sb_append(sb, ", ");
            sb_json_bool(sb, alarm->days[d]);
        }
        sb_append(sb, "]");
    } else {
        sb_append(sb, "null");
    }
    sb_append(sb, "}");
}

void alarms_to_json(sb_t *sb) {
    sb_append(sb, "[");
    for (int i = 0; i < alarm_count; i++) {
        if (i > 0) sb_append(sb, ", ");
        alarm_to_json(&alarms[i], sb);
    }
    sb_append(sb, "]");
}

static bool parse_int(json_parser_t *jp, int min, int max, int *out) {
    float f;
    if (!json_parse_number(jp, &f)) return false;
    if (f != (float)(int)f || (int)f < min || (int)f > max) {
        return json_fail(jp, "number out of range");
    }
    *out = (int)f;
    return true;
}

static bool parse_time(json_parser_t *jp, alarm_t *alarm) {
    bool first = true, has_item;
    int values[2];
    int n = 0;
    if (!json_array_begin(jp)) return false;
    while (json_array_next(jp, &first, &has_item) && has_item) {
        if (n >= 2) return json_fail(jp, "time must be [hour, minute]");
        if (!parse_int(jp, 0, n == 0 ? 23 : 59, &values[n])) return false;
        n++;
    }
    if (jp->error) return false;
    if (n != 2) return json_fail(jp, "time must be [hour, minute]");
    alarm->hour = values[0];
    alarm->minute = values[1];
    return true;
}

static bool parse_days(json_parser_t *jp, alarm_t *alarm) {
    if (json_try_parse_null(jp)) {
        alarm->has_days = false;
        return true;
    }
    bool first = true, has_item;
    int n = 0;
    if (!json_array_begin(jp)) return false;
    while (json_array_next(jp, &first, &has_item) && has_item) {
        if (n >= 7) return json_fail(jp, "days must be array of length 7");
        if (!json_parse_bool(jp, &alarm->days[n])) return json_fail(jp, "days must be array of bools");
        n++;
    }
    if (jp->error) return false;
    if (n != 7) return json_fail(jp, "days must be array of length 7");
    alarm->has_days = true;
    return true;
}

static bool parse_program(json_parser_t *jp, light_program_t *program) {
    bool first = true, has_key;
    bool has_type = false, has_level = false, has_seconds = false;
    char key[16];
    char type_name[32];
    if (!json_object_begin(jp)) return false;
    while (json_object_next(jp, &first, key, sizeof(key), &has_key) && has_key) {
        if (strcmp(key, "type") == 0) {
            if (!json_parse_string(jp, type_name, sizeof(type_name))) return false;
            if (!light_state_from_name(type_name, &program->type)) return json_fail(jp, "unknown state type");
            has_type = true;
        } else if (strcmp(key, "level") == 0) {
            if (!json_parse_number(jp, &program->level)) return false;
            has_level = true;
        } else if (strcmp(key, "seconds") == 0) {
            if (!json_parse_number(jp, &program->seconds)) return false;
            has_seconds = true;
        } else {
            if (!json_skip_value(jp)) return false;
        }
    }
    if (jp->error) return false;
    if (!has_type) return json_fail(jp, "state needs a type");

    switch (program->type) {
        case LIGHT_STATE_RAVE:
            program->level = 0;
            program->seconds = 0;
            return true;
        case LIGHT_STATE_CONSTANT:
            program->seconds = 0;
            break;
        default:
            if (!has_seconds) return json_fail(jp, "state needs seconds");
            if (program->seconds < 0) return json_fail(jp, "seconds must not be negative");
            break;
    }
    if (!has_level) return json_fail(jp, "state needs a level");
    if (program->level < 0 || program->level > 1) return json_fail(jp, "level must be between 0 and 1");
    return true;
}

// Parse fields from JSON into alarm. Fields not present are left alone.
static bool parse_alarm(const char *json, size_t len, alarm_t *alarm, bool *has_time, bool *has_state,
                        bool *has_enabled, char *err, size_t err_cap) {
    json_parser_t jp;
    json_init(&jp, json, len);
    bool first = true, has_key;
    char key[16];
    *has_time = *has_state = *has_enabled = false;

    if (json_object_begin(&jp)) {
        while (json_object_next(&jp, &first, key, sizeof(key), &has_key) && has_key) {
            if (strcmp(key, "time") == 0) {
                *has_time = parse_time(&jp, alarm);
            } else if (strcmp(key, "state") == 0) {
                *has_state = parse_program(&jp, &alarm->program);
            } else if (strcmp(key, "repeat") == 0) {
                json_parse_bool(&jp, &alarm->repeat);
            } else if (strcmp(key, "enabled") == 0) {
                *has_enabled = json_parse_bool(&jp, &alarm->enabled);
            } else if (strcmp(key, "days") == 0) {
                parse_days(&jp, alarm);
            } else {
                json_skip_value(&jp);
            }
        }
        json_expect_end(&jp);
    }
    if (jp.error) {
        snprintf(err, err_cap, "bad alarm: %s", jp.error);
        return false;
    }
    return true;
}

static bool check_index(int *index, char *err, size_t err_cap) {
    // like Python, negative indexes count from the end
    if (*index < 0) *index += alarm_count;
    if (*index < 0 || *index >= alarm_count) {
        snprintf(err, err_cap, "alarm index out of range");
        return false;
    }
    return true;
}

bool alarms_add_json(const char *json, size_t len, char *err, size_t err_cap) {
    if (alarm_count >= MAX_ALARMS) {
        snprintf(err, err_cap, "too many alarms (max %d)", MAX_ALARMS);
        return false;
    }
    alarm_t alarm = {.enabled = true, .repeat = false, .has_days = false};
    bool has_time, has_state, has_enabled;
    if (!parse_alarm(json, len, &alarm, &has_time, &has_state, &has_enabled, err, err_cap)) {
        return false;
    }
    if (!has_time || !has_state) {
        snprintf(err, err_cap, "bad alarm: needs time and state");
        return false;
    }
    reset_last_triggered(&alarm);
    alarms[alarm_count++] = alarm;
    printf("alarm %d added at %02d:%02d\n", alarm_count - 1, alarm.hour, alarm.minute);
    return true;
}

bool alarms_update_json(int index, const char *json, size_t len, char *err, size_t err_cap) {
    if (!check_index(&index, err, err_cap)) {
        return false;
    }
    alarm_t alarm = alarms[index];
    bool has_time, has_state, has_enabled;
    if (!parse_alarm(json, len, &alarm, &has_time, &has_state, &has_enabled, err, err_cap)) {
        return false;
    }
    // as in the MicroPython version, an updated alarm is re-enabled unless told otherwise
    if (!has_enabled) {
        alarm.enabled = true;
    }
    reset_last_triggered(&alarm);
    alarms[index] = alarm;
    printf("alarm %d updated\n", index);
    return true;
}

bool alarms_delete(int index, char *err, size_t err_cap) {
    if (!check_index(&index, err, err_cap)) {
        return false;
    }
    memmove(&alarms[index], &alarms[index + 1], (size_t)(alarm_count - index - 1) * sizeof(alarm_t));
    alarm_count--;
    printf("alarm %d deleted\n", index);
    return true;
}

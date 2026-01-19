

#ifndef UI_HABIT_TRACKER_MANAGE_HABITS_H
#define UI_HABIT_TRACKER_MANAGE_HABITS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[32];
    char description[64];
    uint8_t frequency_type; // 0: weekly, 1: custom
    uint8_t days_selected[7]; // weekly: 0=Mon, 1=Tue, etc.
    uint8_t num_days; 
    uint8_t time_option; // 0: no time, 1: specific time, 2: morn, 3: afternoon, 4: night
    uint8_t hour; 
    uint8_t minute; 
    uint8_t show_in_weekly_todo;
    uint8_t is_active;
    lv_calendar_date_t custom_dates[10]; // frequency -> custom (max 10 dates)
    uint8_t num_custom_dates;
    uint16_t streak_count;
    uint32_t last_completed_date; 
    uint8_t skip_days_used;
    uint32_t skip_days_reset_date; 
    uint8_t is_flexible; 
} habit_t;

extern const uint8_t morn_hr;
extern const uint8_t morn_min;
extern const uint8_t afternoon_hr;
extern const uint8_t afternoon_min;
extern const uint8_t night_hr;
extern const uint8_t night_min;

void ui_manage_habits_init(void);
void ui_manage_habits_show(void);
void ui_manage_habits_hide(void);
void ui_manage_habits_add_predefined(void);
habit_t* ui_manage_habits_get_habit(uint16_t index);
void ui_manage_habits_mark_completed(uint16_t index, bool completed);
uint8_t ui_manage_habits_get_remaining_skips(const habit_t *habit);

#ifdef __cplusplus
}
#endif

#endif // UI_MANAGE_HABITS_H
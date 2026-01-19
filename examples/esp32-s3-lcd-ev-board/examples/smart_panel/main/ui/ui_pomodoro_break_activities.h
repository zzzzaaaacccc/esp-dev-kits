#ifndef UI_POMODORO_BREAK_ACTIVITIES_H
#define UI_POMODORO_BREAK_ACTIVITIES_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ACTIVITY_PHYSICAL_EXERCISE,
    ACTIVITY_NATURE_BREAK,
    ACTIVITY_MINDFULNESS,
    ACTIVITY_RELAXATION,
    ACTIVITY_NONE
} break_activity_t;

void ui_break_activities_init(lv_obj_t *right_panel_container);
void ui_break_activities_show_menu(void);
void ui_break_activities_hide_menu(void);
void ui_break_activities_show_task_list(void);
void ui_break_activities_start_activity(break_activity_t activity);
break_activity_t ui_break_activities_get_selected(void);
void ui_break_activities_exit(void); 

#ifdef __cplusplus
}
#endif

#endif  // UI_POMODORO_BREAK_ACTIVITIES_H

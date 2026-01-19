#ifndef UI_POMODORO_ANALYTICS_H
#define UI_POMODORO_ANALYTICS_H

#include "ui_main.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_pomodoro_analytics_init(void *data);
void ui_pomodoro_analytics_init_in_panel(lv_obj_t *right_panel_container);
void ui_pomodoro_analytics_show(void *data);
void ui_pomodoro_analytics_hide(void *data);
void ui_pomodoro_analytics_update_sessions(uint16_t sessions);
void ui_pomodoro_analytics_update_time(uint16_t minutes);
extern ui_func_desc_t ui_pomodoro_analytics_func;

#ifdef __cplusplus
}
#endif

#endif // UI_POMODORO_ANALYTICS_H
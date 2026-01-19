

#ifndef UI_HABIT_TRACKER_TODAY_VIEW_H
#define UI_HABIT_TRACKER_TODAY_VIEW_H

#ifdef __cplusplus
extern "C" {
#endif


void ui_today_view_init(void);
void ui_today_view_show(void);
void ui_today_view_hide(void);
void ui_today_view_refresh(void);
void ui_today_view_schedule_habit_notifications(void);
void ui_today_view_update_notifications(void);

#ifdef __cplusplus
}
#endif

#endif // UI_TODAY_VIEW_H 
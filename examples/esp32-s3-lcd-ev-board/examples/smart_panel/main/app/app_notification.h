#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NOTIFICATION_TYPE_INFO,
    NOTIFICATION_TYPE_SUCCESS,
    NOTIFICATION_TYPE_WARNING,
    NOTIFICATION_TYPE_ERROR,
    NOTIFICATION_TYPE_HABIT_REMINDER,
    NOTIFICATION_TYPE_CUSTOM
} notification_type_t;

typedef struct {
    const char *title;
    const char *message;
    notification_type_t type;
    bool enable_sound;
    const char *icon;
    void *user_data;
    void (*on_dismiss_callback)(void *user_data);
    void (*on_tap_callback)(void *user_data);
} notification_t;

typedef struct {
    const char *habit_name;
    uint8_t scheduled_hour;
    uint8_t scheduled_minute;
    bool notified_today;
    uint32_t last_notified_time;
} habit_reminder_t;

esp_err_t app_notification_init(void);
esp_err_t app_notification_deinit(void);
esp_err_t app_notification_send(const notification_t *notification);
esp_err_t app_notification_send_simple(const char *title, const char *message, notification_type_t type);
esp_err_t app_notification_send_habit_reminder(const char *habit_name);
esp_err_t app_notification_play_sound(void);
esp_err_t app_notification_dismiss(void);
bool app_notification_is_active(void);
uint8_t app_notification_get_count(void);
esp_err_t app_notification_schedule_habit(const char *habit_name, uint8_t hour, uint8_t minute);
esp_err_t app_notification_check_missed_reminders(void);
esp_err_t app_notification_reset_daily_state(void);

#ifdef __cplusplus
}
#endif
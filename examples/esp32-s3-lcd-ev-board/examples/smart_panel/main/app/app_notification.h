#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// notification data structure
typedef struct {
    const char *habit_name;
    uint32_t duration_ms;
    bool enable_sound;
} notification_t;

// habit reminder schedule structure
typedef struct {
    const char *habit_name;
    uint8_t scheduled_hour;      // 0-23 in 24-hour format
    uint8_t scheduled_minute;    // 0-59
    bool notified_today;         // tracks if notification sent today
    uint32_t last_notified_time; // timestamp of last notification
} habit_reminder_t;

/**
 * @brief initialize notification system
 * @return esp_err_t
 */
esp_err_t app_notification_init(void);

/**
 * @brief deinitialize notification system
 * @return esp_err_t
 */
esp_err_t app_notification_deinit(void);

/**
 * @brief send a habit reminder notification with iphone-style alert
 * @param notification pointer to notification structure with reminder details
 * @return esp_err_t
 */
esp_err_t app_notification_send(const notification_t *notification);

/**
 * @brief send a habit reminder notification when habit time arrives
 * @param habit_name name of the habit to remind about
 * @return esp_err_t
 */
esp_err_t app_notification_send_habit_reminder(const char *habit_name);

/**
 * @brief play notification sound/ringtone
 * @return esp_err_t
 */
esp_err_t app_notification_play_sound(void);

/**
 * @brief dismiss/clear current notification
 * @return esp_err_t
 */
esp_err_t app_notification_dismiss(void);

/**
 * @brief check if notification is currently active
 * @return true if notification is displayed, false otherwise
 */
bool app_notification_is_active(void);

/**
 * @brief get current notification count
 * @return number of pending notifications
 */
uint8_t app_notification_get_count(void);

/**
 * @brief schedule a habit reminder at specific time
 * @param habit_name name of the habit
 * @param hour hour in 24-hour format (0-23)
 * @param minute minute (0-59)
 * @return esp_err_t
 */
esp_err_t app_notification_schedule_habit(const char *habit_name, uint8_t hour, uint8_t minute);

/**
 * @brief check and trigger missed notifications on device startup
 * @return esp_err_t
 */
esp_err_t app_notification_check_missed_reminders(void);

/**
 * @brief update notification state for new day (resets notified flags)
 * @return esp_err_t
 */
esp_err_t app_notification_reset_daily_state(void);

#ifdef __cplusplus
}
#endif

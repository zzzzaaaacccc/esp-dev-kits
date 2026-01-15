#include "app_notification.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <stdio.h>
#include <time.h>

// audio player
#include "audio_player.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "app_notification";

// notification queue and state management
typedef struct {
    notification_t notification;
    uint64_t timestamp;
} notification_queue_item_t;

#define NOTIFICATION_QUEUE_SIZE 10
#define NOTIFICATION_SOUND_DURATION_MS 2000
#define NOTIFICATION_DISPLAY_DURATION_MS 5000
#define NOTIFICATION_ANIMATION_TIME 300

// notification state
static struct {
    QueueHandle_t queue;
    SemaphoreHandle_t mutex;
    TaskHandle_t task_handle;
    bool initialized;
    bool is_active;
    uint8_t notification_count;
    notification_t current_notification;

    lv_obj_t *notification_panel;
    
    // habit reminder scheduling
    habit_reminder_t scheduled_habits[10];
    uint8_t habit_count;
    TaskHandle_t scheduler_task_handle;
    uint32_t current_day;  // tracks day for missed notification detection
} g_notification_state = {
    .initialized = false,
    .is_active = false,
    .notification_count = 0,
    .notification_panel = NULL,
    .habit_count = 0,
    .current_day = 0,
};

// forward declarations
static void notification_task(void *arg);
static void notification_display_iphone_style(const notification_t *notification);
static void notification_hide_with_animation(void);
static esp_err_t notification_load_ringtone(void);
static void notification_scheduler_task(void *arg);
static uint32_t get_current_day_timestamp(void);
static bool is_time_between(uint8_t current_hour, uint8_t current_minute, uint8_t target_hour, uint8_t target_minute, uint8_t window_minutes);

esp_err_t app_notification_init(void)
{
    if (g_notification_state.initialized) {
        return ESP_OK;
    }

    // create queue for notification events
    g_notification_state.queue = xQueueCreate(NOTIFICATION_QUEUE_SIZE, sizeof(notification_queue_item_t));
    if (g_notification_state.queue == NULL) {
        ESP_LOGE(TAG, "failed to create notification queue");
        return ESP_ERR_NO_MEM;
    }

    // create mutex for state protection
    g_notification_state.mutex = xSemaphoreCreateMutex();
    if (g_notification_state.mutex == NULL) {
        ESP_LOGE(TAG, "failed to create notification mutex");
        vQueueDelete(g_notification_state.queue);
        return ESP_ERR_NO_MEM;
    }

    // create notification task
    BaseType_t ret = xTaskCreate(
        notification_task,
        "notification_task",
        4096,
        NULL,
        5,
        &g_notification_state.task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "failed to create notification task");
        vSemaphoreDelete(g_notification_state.mutex);
        vQueueDelete(g_notification_state.queue);
        return ESP_ERR_NO_MEM;
    }

    // create scheduler task for habit reminders
    ret = xTaskCreate(
        notification_scheduler_task,
        "notification_scheduler",
        4096,
        NULL,
        4,
        &g_notification_state.scheduler_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "failed to create scheduler task");
        vTaskDelete(g_notification_state.task_handle);
        vSemaphoreDelete(g_notification_state.mutex);
        vQueueDelete(g_notification_state.queue);
        return ESP_ERR_NO_MEM;
    }

    g_notification_state.initialized = true;
    ESP_LOGI(TAG, "notification system initialized");

    return ESP_OK;
}

esp_err_t app_notification_deinit(void)
{
    if (!g_notification_state.initialized) {
        return ESP_OK;
    }

    // stop notification task
    if (g_notification_state.task_handle != NULL) {
        vTaskDelete(g_notification_state.task_handle);
        g_notification_state.task_handle = NULL;
    }

    // stop scheduler task
    if (g_notification_state.scheduler_task_handle != NULL) {
        vTaskDelete(g_notification_state.scheduler_task_handle);
        g_notification_state.scheduler_task_handle = NULL;
    }

    // cleanup queue and mutex
    if (g_notification_state.queue != NULL) {
        vQueueDelete(g_notification_state.queue);
        g_notification_state.queue = NULL;
    }

    if (g_notification_state.mutex != NULL) {
        vSemaphoreDelete(g_notification_state.mutex);
        g_notification_state.mutex = NULL;
    }

    g_notification_state.initialized = false;
    ESP_LOGI(TAG, "notification system deinitialized");

    return ESP_OK;
}

esp_err_t app_notification_send(const notification_t *notification)
{
    if (!g_notification_state.initialized) {
        ESP_LOGE(TAG, "notification system not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (notification == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    notification_queue_item_t item;
    item.notification = *notification;
    item.timestamp = esp_timer_get_time();

    // add to queue
    if (xQueueSend(g_notification_state.queue, &item, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG, "notification queue full, dropping notification");
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
    g_notification_state.notification_count++;
    xSemaphoreGive(g_notification_state.mutex);

    ESP_LOGI(TAG, "notification queued: %s", notification->habit_name);

    return ESP_OK;
}

esp_err_t app_notification_send_habit_reminder(const char *habit_name)
{
    if (habit_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    notification_t notification = {
        .habit_name = habit_name,
        .duration_ms = NOTIFICATION_DISPLAY_DURATION_MS,
        .enable_sound = true,
    };

    return app_notification_send(&notification);
}

esp_err_t app_notification_play_sound(void)
{
    if (!g_notification_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // attempt to play notification sound from spiffs
    FILE *fp = fopen("/spiffs/notification.wav", "rb");
    if (fp != NULL) {
        esp_err_t ret = audio_player_play(fp);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "failed to play notification sound");
            fclose(fp);
        }
    } else {
        ESP_LOGW(TAG, "notification sound file not found");
    }

    return ESP_OK;
}

esp_err_t app_notification_dismiss(void)
{
    xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
    g_notification_state.is_active = false;
    xSemaphoreGive(g_notification_state.mutex);

    ESP_LOGI(TAG, "notification dismissed");

    return ESP_OK;
}

bool app_notification_is_active(void)
{
    xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
    bool is_active = g_notification_state.is_active;
    xSemaphoreGive(g_notification_state.mutex);

    return is_active;
}

uint8_t app_notification_get_count(void)
{
    xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
    uint8_t count = g_notification_state.notification_count;
    xSemaphoreGive(g_notification_state.mutex);

    return count;
}

esp_err_t app_notification_schedule_habit(const char *habit_name, uint8_t hour, uint8_t minute)
{
    if (!g_notification_state.initialized || habit_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (hour > 23 || minute > 59) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
    
    if (g_notification_state.habit_count >= 10) {
        xSemaphoreGive(g_notification_state.mutex);
        ESP_LOGW(TAG, "max habits scheduled, cannot add more");
        return ESP_ERR_NO_MEM;
    }

    // add habit to schedule
    g_notification_state.scheduled_habits[g_notification_state.habit_count].habit_name = habit_name;
    g_notification_state.scheduled_habits[g_notification_state.habit_count].scheduled_hour = hour;
    g_notification_state.scheduled_habits[g_notification_state.habit_count].scheduled_minute = minute;
    g_notification_state.scheduled_habits[g_notification_state.habit_count].notified_today = false;
    g_notification_state.scheduled_habits[g_notification_state.habit_count].last_notified_time = 0;
    
    g_notification_state.habit_count++;
    
    xSemaphoreGive(g_notification_state.mutex);
    
    ESP_LOGI(TAG, "habit scheduled: %s at %02d:%02d", habit_name, hour, minute);
    return ESP_OK;
}

esp_err_t app_notification_check_missed_reminders(void)
{
    if (!g_notification_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
    
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    // check each scheduled habit for missed notifications
    for (uint8_t i = 0; i < g_notification_state.habit_count; i++) {
        habit_reminder_t *habit = &g_notification_state.scheduled_habits[i];
        
        // if habit time has passed today and we haven't notified yet, send reminder
        if (!habit->notified_today) {
            uint8_t current_hour = timeinfo.tm_hour;
            uint8_t current_minute = timeinfo.tm_min;
            
            // check if current time is at or past the scheduled time
            if ((current_hour > habit->scheduled_hour) || 
                (current_hour == habit->scheduled_hour && current_minute >= habit->scheduled_minute)) {
                
                xSemaphoreGive(g_notification_state.mutex);
                
                // send missed notification
                ESP_LOGI(TAG, "sending missed reminder for habit: %s", habit->habit_name);
                app_notification_send_habit_reminder(habit->habit_name);
                
                xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
                habit->notified_today = true;
                habit->last_notified_time = now;
            }
        }
    }
    
    xSemaphoreGive(g_notification_state.mutex);
    return ESP_OK;
}

esp_err_t app_notification_reset_daily_state(void)
{
    if (!g_notification_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
    
    // reset notified flags for new day
    for (uint8_t i = 0; i < g_notification_state.habit_count; i++) {
        g_notification_state.scheduled_habits[i].notified_today = false;
    }
    
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    g_notification_state.current_day = timeinfo.tm_mday;
    
    xSemaphoreGive(g_notification_state.mutex);
    
    ESP_LOGI(TAG, "daily notification state reset");
    return ESP_OK;
}

// ============================================================================
// internal functions
// ============================================================================

static void notification_task(void *arg)
{
    notification_queue_item_t item;

    ESP_LOGI(TAG, "notification task started");

    while (1) {
        // wait for notification in queue
        if (xQueueReceive(g_notification_state.queue, &item, pdMS_TO_TICKS(100)) == pdTRUE) {
            xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
            g_notification_state.is_active = true;
            g_notification_state.current_notification = item.notification;
            xSemaphoreGive(g_notification_state.mutex);

            // display notification with iphone-style animation
            notification_display_iphone_style(&item.notification);

            // play sound if enabled
            if (item.notification.enable_sound) {
                app_notification_play_sound();
            }

            // wait for notification duration then auto-dismiss
            vTaskDelay(pdMS_TO_TICKS(item.notification.duration_ms));

            // hide notification with animation
            notification_hide_with_animation();

            xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
            g_notification_state.is_active = false;
            g_notification_state.notification_count = (g_notification_state.notification_count > 0) ?
                                                      (g_notification_state.notification_count - 1) : 0;
            xSemaphoreGive(g_notification_state.mutex);

            ESP_LOGI(TAG, "notification displayed and dismissed");
        }
    }
}

static void notification_dismiss_anim_cb(lv_anim_t *a)
{
    // called when dismiss animation finishes
    if (g_notification_state.notification_panel != NULL) {
        lv_obj_del(g_notification_state.notification_panel);
        g_notification_state.notification_panel = NULL;
    }
}

static void notification_display_iphone_style(const notification_t *notification)
{
    // create notification panel with iphone-style appearance
    lv_obj_t *scr = lv_scr_act();
    
    // create main notification panel
    g_notification_state.notification_panel = lv_obj_create(scr, NULL);
    lv_obj_set_width(g_notification_state.notification_panel, 300);
    lv_obj_set_height(g_notification_state.notification_panel, 80);
    lv_obj_align(g_notification_state.notification_panel, NULL, LV_ALIGN_IN_TOP_MID, 0, 10);
    
    // set notification panel style - dark background with rounded corners
    lv_obj_set_style_local_bg_color(g_notification_state.notification_panel, LV_OBJ_PART_MAIN, 
                                     LV_STATE_DEFAULT, lv_color_hex(0x000000));
    lv_obj_set_style_local_bg_opa(g_notification_state.notification_panel, LV_OBJ_PART_MAIN, 
                                   LV_STATE_DEFAULT, LV_OPA_70);
    lv_obj_set_style_local_radius(g_notification_state.notification_panel, LV_OBJ_PART_MAIN, 
                                   LV_STATE_DEFAULT, 15);
    lv_obj_set_style_local_border_width(g_notification_state.notification_panel, LV_OBJ_PART_MAIN, 
                                         LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_shadow_width(g_notification_state.notification_panel, LV_OBJ_PART_MAIN, 
                                         LV_STATE_DEFAULT, 10);
    
    // create habit name label
    lv_obj_t *habit_label = lv_label_create(g_notification_state.notification_panel, NULL);
    lv_label_set_text(habit_label, notification->habit_name);
    lv_obj_set_style_local_text_color(habit_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 
                                       lv_color_hex(0xFFFFFF));
    lv_obj_align(habit_label, g_notification_state.notification_panel, LV_ALIGN_IN_TOP_LEFT, 15, 8);
    
    // create reminder message label
    lv_obj_t *msg_label = lv_label_create(g_notification_state.notification_panel, NULL);
    lv_label_set_text(msg_label, "time to complete this habit now!");
    lv_obj_set_style_local_text_color(msg_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 
                                       lv_color_hex(0xCCCCCC));
    lv_obj_align(msg_label, g_notification_state.notification_panel, LV_ALIGN_IN_TOP_LEFT, 15, 35);
    
    // animate notification sliding down from top
    lv_anim_t slide_anim;
    lv_anim_init(&slide_anim);
    lv_anim_set_var(&slide_anim, g_notification_state.notification_panel);
    lv_anim_set_values(&slide_anim, -100, 10);
    lv_anim_set_time(&slide_anim, NOTIFICATION_ANIMATION_TIME);
    lv_anim_set_exec_cb(&slide_anim, (lv_anim_exec_xcb_t) lv_obj_set_y);
    lv_anim_start(&slide_anim);
    
    ESP_LOGI(TAG, "notification displayed: %s", notification->habit_name);
}

static void notification_hide_with_animation(void)
{
    if (g_notification_state.notification_panel == NULL) {
        return;
    }
    
    // animate notification sliding up and out of view
    lv_anim_t slide_anim;
    lv_anim_init(&slide_anim);
    lv_anim_set_var(&slide_anim, g_notification_state.notification_panel);
    lv_anim_set_values(&slide_anim, 10, -100);
    lv_anim_set_time(&slide_anim, NOTIFICATION_ANIMATION_TIME);
    lv_anim_set_exec_cb(&slide_anim, (lv_anim_exec_xcb_t) lv_obj_set_y);
    lv_anim_set_ready_cb(&slide_anim, notification_dismiss_anim_cb);
    lv_anim_start(&slide_anim);
}

static esp_err_t notification_load_ringtone(void)
{
    // load notification ringtone from spiffs
    // returns ESP_OK if successful, otherwise uses system default

    return ESP_OK;
}

// habit reminder scheduler task - runs periodically to check scheduled times
static void notification_scheduler_task(void *arg)
{
    ESP_LOGI(TAG, "habit reminder scheduler started");
    
    TickType_t last_check_time = xTaskGetTickCount();
    
    while (1) {
        // check every 60 seconds for scheduled reminders
        vTaskDelay(pdMS_TO_TICKS(60000));
        
        xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
        
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        uint8_t current_hour = timeinfo.tm_hour;
        uint8_t current_minute = timeinfo.tm_min;
        
        // check if day changed and reset state
        if (timeinfo.tm_mday != g_notification_state.current_day) {
            for (uint8_t i = 0; i < g_notification_state.habit_count; i++) {
                g_notification_state.scheduled_habits[i].notified_today = false;
            }
            g_notification_state.current_day = timeinfo.tm_mday;
            ESP_LOGI(TAG, "day changed, reset notification state");
        }
        
        xSemaphoreGive(g_notification_state.mutex);
        
        // check each habit schedule
        for (uint8_t i = 0; i < g_notification_state.habit_count; i++) {
            xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
            habit_reminder_t *habit = &g_notification_state.scheduled_habits[i];
            bool should_notify = false;
            
            // check if time matches (within 1 minute window)
            if (!habit->notified_today && 
                current_hour == habit->scheduled_hour && 
                current_minute == habit->scheduled_minute) {
                should_notify = true;
                habit->notified_today = true;
                habit->last_notified_time = now;
            }
            
            xSemaphoreGive(g_notification_state.mutex);
            
            if (should_notify) {
                ESP_LOGI(TAG, "habit reminder time reached: %s", habit->habit_name);
                app_notification_send_habit_reminder(habit->habit_name);
            }
        }
    }
}

// helper function to get current day timestamp
static uint32_t get_current_day_timestamp(void)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return timeinfo.tm_mday;
}

// helper function to check if time is within window
static bool is_time_between(uint8_t current_hour, uint8_t current_minute, uint8_t target_hour, uint8_t target_minute, uint8_t window_minutes)
{
    // convert times to minutes for easier comparison
    uint32_t current_time = current_hour * 60 + current_minute;
    uint32_t target_time = target_hour * 60 + target_minute;
    
    return (current_time >= target_time) && (current_time < target_time + window_minutes);
}

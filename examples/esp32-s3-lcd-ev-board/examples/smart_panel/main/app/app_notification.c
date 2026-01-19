#include "app_notification.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <stdio.h>
#include <time.h>

#include "audio_player.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "app_notification";

typedef struct {
    notification_t notification;
    uint64_t timestamp;
} notification_queue_item_t;

#define NOTIFICATION_QUEUE_SIZE 10
#define NOTIFICATION_ANIMATION_TIME 300
#define SWIPE_THRESHOLD 50
#define NOTIFICATION_AUTO_DISMISS_MS 10000

static struct {
    QueueHandle_t queue;
    SemaphoreHandle_t mutex;
    TaskHandle_t task_handle;
    bool initialized;
    bool is_active;
    uint8_t notification_count;
    notification_t current_notification;

    lv_obj_t *notification_panel;
    lv_point_t touch_start;
    int16_t panel_start_y;
    lv_task_t *auto_dismiss_task;
    
    habit_reminder_t scheduled_habits[10];
    uint8_t habit_count;
    TaskHandle_t scheduler_task_handle;
    uint32_t current_day;
} g_notification_state = {
    .initialized = false,
    .is_active = false,
    .notification_count = 0,
    .notification_panel = NULL,
    .auto_dismiss_task = NULL,
    .habit_count = 0,
    .current_day = 0,
};

static void notification_task(void *arg);
static void notification_display_iphone_style(const notification_t *notification);
static void notification_hide_with_animation(void);
static void notification_scheduler_task(void *arg);
static void notification_gesture_handler(lv_obj_t *obj, lv_event_t event);
static lv_color_t get_notification_color(notification_type_t type);
static const char* get_notification_icon(notification_type_t type);

esp_err_t app_notification_init(void)
{
    if (g_notification_state.initialized) {
        return ESP_OK;
    }

    g_notification_state.queue = xQueueCreate(NOTIFICATION_QUEUE_SIZE, sizeof(notification_queue_item_t));
    if (g_notification_state.queue == NULL) {
        ESP_LOGE(TAG, "failed to create notification queue");
        return ESP_ERR_NO_MEM;
    }

    g_notification_state.mutex = xSemaphoreCreateMutex();
    if (g_notification_state.mutex == NULL) {
        ESP_LOGE(TAG, "failed to create notification mutex");
        vQueueDelete(g_notification_state.queue);
        return ESP_ERR_NO_MEM;
    }

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

    if (g_notification_state.task_handle != NULL) {
        vTaskDelete(g_notification_state.task_handle);
        g_notification_state.task_handle = NULL;
    }

    if (g_notification_state.scheduler_task_handle != NULL) {
        vTaskDelete(g_notification_state.scheduler_task_handle);
        g_notification_state.scheduler_task_handle = NULL;
    }

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

    if (notification == NULL || notification->title == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
    
    UBaseType_t queue_count = uxQueueMessagesWaiting(g_notification_state.queue);
    
    if (queue_count > 0) {
        notification_queue_item_t temp_item;
        notification_queue_item_t items[NOTIFICATION_QUEUE_SIZE];
        uint8_t item_count = 0;
        
        while (xQueueReceive(g_notification_state.queue, &temp_item, 0) == pdTRUE) {
            bool is_duplicate = false;
            if (strcmp(temp_item.notification.title, notification->title) == 0) {
                is_duplicate = true;
            }
            
            if (!is_duplicate) {
                items[item_count++] = temp_item;
            }
        }
        
        for (uint8_t i = 0; i < item_count; i++) {
            xQueueSend(g_notification_state.queue, &items[i], 0);
        }
    }
    
    xSemaphoreGive(g_notification_state.mutex);

    notification_queue_item_t item;
    item.notification = *notification;
    item.timestamp = esp_timer_get_time();

    if (xQueueSend(g_notification_state.queue, &item, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG, "notification queue full, dropping notification");
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
    g_notification_state.notification_count++;
    xSemaphoreGive(g_notification_state.mutex);

    ESP_LOGI(TAG, "notification queued: %s", notification->title);

    return ESP_OK;
}

esp_err_t app_notification_send_simple(const char *title, const char *message, notification_type_t type)
{
    if (title == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    notification_t notification = {
        .title = title,
        .message = message,
        .type = type,
        .enable_sound = (type == NOTIFICATION_TYPE_ERROR || type == NOTIFICATION_TYPE_WARNING),
        .icon = NULL,
        .user_data = NULL,
        .on_dismiss_callback = NULL,
        .on_tap_callback = NULL
    };

    return app_notification_send(&notification);
}

esp_err_t app_notification_send_habit_reminder(const char *habit_name)
{
    if (habit_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    notification_t notification = {
        .title = habit_name,
        .message = "time to complete this habit now!",
        .type = NOTIFICATION_TYPE_HABIT_REMINDER,
        .enable_sound = true,
        .icon = LV_SYMBOL_BELL,
        .user_data = NULL,
        .on_dismiss_callback = NULL,
        .on_tap_callback = NULL
    };

    return app_notification_send(&notification);
}

esp_err_t app_notification_play_sound(void)
{
    if (!g_notification_state.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

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
    if (g_notification_state.current_notification.on_dismiss_callback) {
        g_notification_state.current_notification.on_dismiss_callback(
            g_notification_state.current_notification.user_data
        );
    }
    
    notification_hide_with_animation();
    
    xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
    g_notification_state.is_active = false;
    g_notification_state.notification_count = (g_notification_state.notification_count > 0) ?
                                              (g_notification_state.notification_count - 1) : 0;
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
    
    for (uint8_t i = 0; i < g_notification_state.habit_count; i++) {
        habit_reminder_t *habit = &g_notification_state.scheduled_habits[i];
        
        if (!habit->notified_today) {
            uint8_t current_hour = timeinfo.tm_hour;
            uint8_t current_minute = timeinfo.tm_min;
            
            if ((current_hour > habit->scheduled_hour) || 
                (current_hour == habit->scheduled_hour && current_minute >= habit->scheduled_minute)) {
                
                xSemaphoreGive(g_notification_state.mutex);
                
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

static lv_color_t get_notification_color(notification_type_t type)
{
    switch (type) {
        case NOTIFICATION_TYPE_SUCCESS:
            return lv_color_hex(0x00AA00);
        case NOTIFICATION_TYPE_WARNING:
            return lv_color_hex(0xFF9500);
        case NOTIFICATION_TYPE_ERROR:
            return lv_color_hex(0xFF3B30);
        case NOTIFICATION_TYPE_INFO:
            return lv_color_hex(0x007AFF);
        case NOTIFICATION_TYPE_HABIT_REMINDER:
            return lv_color_hex(0x5856D6);
        case NOTIFICATION_TYPE_CUSTOM:
        default:
            return lv_color_hex(0x000000);
    }
}

static const char* get_notification_icon(notification_type_t type)
{
    switch (type) {
        case NOTIFICATION_TYPE_SUCCESS:
            return LV_SYMBOL_OK;
        case NOTIFICATION_TYPE_WARNING:
            return LV_SYMBOL_WARNING;
        case NOTIFICATION_TYPE_ERROR:
            return LV_SYMBOL_CLOSE;
        case NOTIFICATION_TYPE_INFO:
            return LV_SYMBOL_HOME;
        case NOTIFICATION_TYPE_HABIT_REMINDER:
            return LV_SYMBOL_BELL;
        case NOTIFICATION_TYPE_CUSTOM:
        default:
            return NULL;
    }
}

static void notification_task(void *arg)
{
    notification_queue_item_t item;

    ESP_LOGI(TAG, "notification task started");

    while (1) {
        if (xQueueReceive(g_notification_state.queue, &item, pdMS_TO_TICKS(100)) == pdTRUE) {
            xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
            g_notification_state.is_active = true;
            g_notification_state.current_notification = item.notification;
            xSemaphoreGive(g_notification_state.mutex);

            notification_display_iphone_style(&item.notification);

            if (item.notification.enable_sound) {
                app_notification_play_sound();
            }

            ESP_LOGI(TAG, "notification displayed, waiting for user swipe");
        }
    }
}

static void notification_dismiss_anim_cb(lv_anim_t *a)
{
    if (g_notification_state.notification_panel != NULL) {
        if (g_notification_state.auto_dismiss_task != NULL) {
            lv_task_del(g_notification_state.auto_dismiss_task);
            g_notification_state.auto_dismiss_task = NULL;
        }
        lv_obj_del(g_notification_state.notification_panel);
        g_notification_state.notification_panel = NULL;
    }
}

static void notification_tap_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED) {
        if (g_notification_state.current_notification.on_tap_callback) {
            g_notification_state.current_notification.on_tap_callback(
                g_notification_state.current_notification.user_data
            );
        }
    }
}

static void auto_dismiss_task_cb(lv_task_t *task)
{
    app_notification_dismiss();
}

static void notification_gesture_handler(lv_obj_t *obj, lv_event_t event)
{
    static lv_point_t start_point;
    static int16_t start_y_pos;
    static bool is_dragging = false;
    
    if (event == LV_EVENT_PRESSED) {
        lv_indev_t *indev = lv_indev_get_act();
        lv_indev_get_point(indev, &start_point);
        start_y_pos = lv_obj_get_y(obj);
        g_notification_state.panel_start_y = start_y_pos;
        is_dragging = false;
    }
    else if (event == LV_EVENT_PRESSING) {
        lv_indev_t *indev = lv_indev_get_act();
        lv_point_t current_point;
        lv_indev_get_point(indev, &current_point);
        
        int16_t diff_y = current_point.y - start_point.y;
        
        if (diff_y < -5) {
            is_dragging = true;
        }
        
        if (diff_y < 0) {
            lv_obj_set_y(obj, start_y_pos + diff_y);
        }
    }
    else if (event == LV_EVENT_RELEASED || event == LV_EVENT_PRESS_LOST) {
        lv_indev_t *indev = lv_indev_get_act();
        lv_point_t current_point;
        lv_indev_get_point(indev, &current_point);
        
        int16_t diff_y = current_point.y - start_point.y;
        
        if (is_dragging && diff_y < -SWIPE_THRESHOLD) {
            app_notification_dismiss();
        } else {
            lv_anim_t bounce_anim;
            lv_anim_init(&bounce_anim);
            lv_anim_set_var(&bounce_anim, obj);
            lv_anim_set_values(&bounce_anim, lv_obj_get_y(obj), g_notification_state.panel_start_y);
            lv_anim_set_time(&bounce_anim, 200);
            lv_anim_set_exec_cb(&bounce_anim, (lv_anim_exec_xcb_t) lv_obj_set_y);
            lv_anim_start(&bounce_anim);
        }
        
        is_dragging = false;
    }
}

static void notification_display_iphone_style(const notification_t *notification)
{
    lv_obj_t *scr = lv_scr_act();
    
    g_notification_state.notification_panel = lv_obj_create(scr, NULL);
    lv_obj_set_width(g_notification_state.notification_panel, 300);
    lv_obj_set_height(g_notification_state.notification_panel, 80);
    lv_obj_align(g_notification_state.notification_panel, NULL, LV_ALIGN_IN_TOP_MID, 0, 10);
    
    lv_color_t bg_color = get_notification_color(notification->type);
    
    lv_obj_set_style_local_bg_color(g_notification_state.notification_panel, LV_OBJ_PART_MAIN, 
                                     LV_STATE_DEFAULT, bg_color);
    lv_obj_set_style_local_bg_opa(g_notification_state.notification_panel, LV_OBJ_PART_MAIN, 
                                   LV_STATE_DEFAULT, LV_OPA_90);
    lv_obj_set_style_local_radius(g_notification_state.notification_panel, LV_OBJ_PART_MAIN, 
                                   LV_STATE_DEFAULT, 15);
    lv_obj_set_style_local_border_width(g_notification_state.notification_panel, LV_OBJ_PART_MAIN, 
                                         LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_shadow_width(g_notification_state.notification_panel, LV_OBJ_PART_MAIN, 
                                         LV_STATE_DEFAULT, 10);
    
    const char *icon = notification->icon ? notification->icon : get_notification_icon(notification->type);
    int16_t content_x_offset = 15;
    
    if (icon != NULL) {
        lv_obj_t *icon_label = lv_label_create(g_notification_state.notification_panel, NULL);
        lv_label_set_text(icon_label, icon);
        lv_obj_set_style_local_text_color(icon_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 
                                           lv_color_hex(0xFFFFFF));
        lv_obj_align(icon_label, g_notification_state.notification_panel, LV_ALIGN_IN_LEFT_MID, 10, 0);
        content_x_offset = 35;
    }
    
    lv_obj_t *title_label = lv_label_create(g_notification_state.notification_panel, NULL);
    lv_label_set_text(title_label, notification->title);
    lv_obj_set_style_local_text_color(title_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 
                                       lv_color_hex(0xFFFFFF));
    lv_obj_align(title_label, g_notification_state.notification_panel, LV_ALIGN_IN_TOP_LEFT, content_x_offset, 8);
    
    if (notification->message != NULL && strlen(notification->message) > 0) {
        lv_obj_t *msg_label = lv_label_create(g_notification_state.notification_panel, NULL);
        lv_label_set_text(msg_label, notification->message);
        lv_obj_set_style_local_text_color(msg_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, 
                                           lv_color_hex(0xEEEEEE));
        lv_obj_align(msg_label, g_notification_state.notification_panel, LV_ALIGN_IN_TOP_LEFT, content_x_offset, 35);
    }
    
    lv_obj_set_event_cb(g_notification_state.notification_panel, notification_gesture_handler);
    lv_obj_set_click(g_notification_state.notification_panel, true);
    
    if (notification->on_tap_callback) {
        lv_obj_add_protect(g_notification_state.notification_panel, LV_PROTECT_CLICK_FOCUS);
    }
    
    lv_anim_t slide_anim;
    lv_anim_init(&slide_anim);
    lv_anim_set_var(&slide_anim, g_notification_state.notification_panel);
    lv_anim_set_values(&slide_anim, -100, 10);
    lv_anim_set_time(&slide_anim, NOTIFICATION_ANIMATION_TIME);
    lv_anim_set_exec_cb(&slide_anim, (lv_anim_exec_xcb_t) lv_obj_set_y);
    lv_anim_start(&slide_anim);
    
    g_notification_state.panel_start_y = 10;
    
    if (g_notification_state.auto_dismiss_task != NULL) {
        lv_task_del(g_notification_state.auto_dismiss_task);
    }
    g_notification_state.auto_dismiss_task = lv_task_create(auto_dismiss_task_cb, NOTIFICATION_AUTO_DISMISS_MS, LV_TASK_PRIO_LOW, NULL);
    lv_task_once(g_notification_state.auto_dismiss_task);
    
    ESP_LOGI(TAG, "notification displayed: %s - %s", notification->title, 
             notification->message ? notification->message : "");
}

static void notification_hide_with_animation(void)
{
    if (g_notification_state.notification_panel == NULL) {
        return;
    }
    
    if (g_notification_state.auto_dismiss_task != NULL) {
        lv_task_del(g_notification_state.auto_dismiss_task);
        g_notification_state.auto_dismiss_task = NULL;
    }
    
    lv_anim_t slide_anim;
    lv_anim_init(&slide_anim);
    lv_anim_set_var(&slide_anim, g_notification_state.notification_panel);
    lv_anim_set_values(&slide_anim, lv_obj_get_y(g_notification_state.notification_panel), -100);
    lv_anim_set_time(&slide_anim, NOTIFICATION_ANIMATION_TIME);
    lv_anim_set_exec_cb(&slide_anim, (lv_anim_exec_xcb_t) lv_obj_set_y);
    lv_anim_set_ready_cb(&slide_anim, notification_dismiss_anim_cb);
    lv_anim_start(&slide_anim);
}

static void notification_scheduler_task(void *arg)
{
    ESP_LOGI(TAG, "habit reminder scheduler started");
    
    TickType_t last_check_time = xTaskGetTickCount();
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        
        xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
        
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        uint8_t current_hour = timeinfo.tm_hour;
        uint8_t current_minute = timeinfo.tm_min;
        
        if (timeinfo.tm_mday != g_notification_state.current_day) {
            for (uint8_t i = 0; i < g_notification_state.habit_count; i++) {
                g_notification_state.scheduled_habits[i].notified_today = false;
            }
            g_notification_state.current_day = timeinfo.tm_mday;
            ESP_LOGI(TAG, "day changed, reset notification state");
        }
        
        for (uint8_t i = 0; i < g_notification_state.habit_count; i++) {
            habit_reminder_t *habit = &g_notification_state.scheduled_habits[i];
            bool should_notify = false;
            
            if (!habit->notified_today && 
                current_hour == habit->scheduled_hour && 
                current_minute == habit->scheduled_minute) {
                should_notify = true;
                habit->notified_today = true;
                habit->last_notified_time = now;
            }
            
            if (should_notify) {
                xSemaphoreGive(g_notification_state.mutex);
                ESP_LOGI(TAG, "habit reminder time reached: %s", habit->habit_name);
                app_notification_send_habit_reminder(habit->habit_name);
                xSemaphoreTake(g_notification_state.mutex, portMAX_DELAY);
            }
        }
        
        xSemaphoreGive(g_notification_state.mutex);
    }
}
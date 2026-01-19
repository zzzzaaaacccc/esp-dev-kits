#include "ui_pomodoro_task_list.h"
#include "ui_main.h"
#include "ui_font_declare.h"
#include "ui_keyboard.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static const char *TAG = "UI_TASK_LIST";

static task_t _tasks[MAX_TASKS];
static uint16_t _task_count = 0;
static lv_obj_t *_textarea_task = NULL;
static lv_obj_t *_btn_add_task = NULL;
static lv_obj_t *_task_container = NULL;  // Changed from _task_list to _task_container
static lv_obj_t *_keyboard = NULL;
static lv_obj_t *_input_cont = NULL;
static lv_obj_t *_task_root_cont = NULL;

/* Forward declarations */
static void _textarea_focus_cb(lv_obj_t *obj, lv_event_t event);
static void _btn_add_task_cb(lv_obj_t *obj, lv_event_t event);
static void _checkbox_event_cb(lv_obj_t *obj, lv_event_t event);
static void _refresh_task_display(void);

static void _textarea_focus_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_FOCUSED || event == LV_EVENT_CLICKED) {
        if (!_keyboard) {
            _keyboard = ui_keyboard_create(obj);
        }
    }
    else if (event == LV_EVENT_DEFOCUSED || event == LV_EVENT_APPLY) {
        if (_keyboard) {
            ui_keyboard_delete(_keyboard);
            _keyboard = NULL;
        }
    }
}

static void _btn_add_task_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        const char *task_text = lv_textarea_get_text(_textarea_task);
        if (task_text && strlen(task_text) > 0) {
            ui_task_list_add_task(task_text);
            lv_textarea_set_text(_textarea_task, "");
        }
    }
}

static void _checkbox_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_VALUE_CHANGED) {
        if (lv_checkbox_is_checked(obj)) {
            // Get the task index stored in the checkbox
            uint16_t index = (uint16_t)(uintptr_t)lv_obj_get_user_data(obj);
            ui_task_list_remove_task(index);
        }
    }
}

static void _refresh_task_display(void) {
    lv_obj_clean(_task_container);

    for (uint16_t i = 0; i < _task_count; i++) {
        lv_obj_t *task_cont = lv_cont_create(_task_container, NULL);
        lv_cont_set_layout(task_cont, LV_LAYOUT_ROW_MID);
        lv_obj_set_width(task_cont, lv_obj_get_width(_task_container) - 40);
        lv_cont_set_fit2(task_cont, LV_FIT_PARENT, LV_FIT_TIGHT);  // Height fits content
        lv_obj_set_style_local_pad_inner(task_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 10);
        lv_obj_set_style_local_border_width(task_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 1);
        lv_obj_set_style_local_border_color(task_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(200, 200, 200));
        lv_obj_set_style_local_radius(task_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 5);

        lv_obj_t *checkbox = lv_checkbox_create(task_cont, NULL);
        lv_checkbox_set_text(checkbox, _tasks[i].text);
        lv_obj_set_event_cb(checkbox, _checkbox_event_cb);
        lv_obj_set_user_data(checkbox, (void*)(uintptr_t)i);
        lv_obj_set_width(checkbox, lv_obj_get_width(_task_container) - 80);
        lv_obj_t *label = lv_obj_get_child(checkbox, NULL);
        if (label) {
            lv_label_set_long_mode(label, LV_LABEL_LONG_BREAK);
        }
    }
}

void ui_task_list_init(lv_obj_t *parent_container) {
    lv_obj_t *scrl = lv_page_get_scrl(parent_container);
    int scrl_width = scrl ? lv_obj_get_width(scrl) : 0;
    if (scrl_width <= 0) {
        scrl_width = lv_obj_get_width(parent_container) - 10;
    }
    
    // Root container for the task tab, keeps input + list stacked at the top
    _task_root_cont = lv_cont_create(scrl, NULL);
    lv_obj_set_width(_task_root_cont, scrl_width - 10);
    lv_obj_align(_task_root_cont, NULL, LV_ALIGN_IN_TOP_LEFT, 5, 0);
    lv_cont_set_layout(_task_root_cont, LV_LAYOUT_COLUMN_LEFT);
    lv_cont_set_fit2(_task_root_cont, LV_FIT_NONE, LV_FIT_TIGHT);
    lv_obj_set_style_local_bg_opa(_task_root_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_width(_task_root_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_all(_task_root_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_inner(_task_root_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 5);

    _input_cont = lv_cont_create(_task_root_cont, NULL);
    lv_obj_set_width(_input_cont, lv_obj_get_width(_task_root_cont));
    lv_cont_set_layout(_input_cont, LV_LAYOUT_ROW_MID);
    lv_cont_set_fit2(_input_cont, LV_FIT_NONE, LV_FIT_TIGHT);  // Fixed width, tight height
    lv_obj_set_style_local_pad_inner(_input_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 5);
    lv_obj_set_style_local_border_width(_input_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_bg_opa(_input_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);

    _textarea_task = lv_textarea_create(_input_cont, NULL);
    lv_obj_set_size(_textarea_task, scrl_width - 130, 40);
    lv_textarea_set_placeholder_text(_textarea_task, "Enter task...");
    lv_textarea_set_text(_textarea_task, "");
    lv_obj_set_event_cb(_textarea_task, _textarea_focus_cb);

    // add task button
    _btn_add_task = lv_btn_create(_input_cont, NULL);
    lv_obj_set_size(_btn_add_task, 80, 40);
    lv_obj_set_event_cb(_btn_add_task, _btn_add_task_cb);
    lv_obj_set_style_local_value_str(_btn_add_task, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, "Add");

    _task_container = lv_cont_create(_task_root_cont, NULL);
    lv_obj_set_width(_task_container, lv_obj_get_width(_task_root_cont));
    lv_cont_set_fit2(_task_container, LV_FIT_NONE, LV_FIT_TIGHT);  // Fixed width, tight height
    lv_cont_set_layout(_task_container, LV_LAYOUT_COLUMN_LEFT);
    lv_obj_set_style_local_bg_opa(_task_container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_width(_task_container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_all(_task_container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_inner(_task_container, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);

    lv_obj_set_hidden(_task_root_cont, true);
}

void ui_task_list_show(void) {
    if (_task_root_cont) {
        lv_obj_set_hidden(_task_root_cont, false);
    }

    if (_input_cont) {
        lv_obj_set_hidden(_input_cont, false);
    }
    if (_textarea_task) {
        lv_obj_set_hidden(_textarea_task, false);
    }
    if (_btn_add_task) {
        lv_obj_set_hidden(_btn_add_task, false);
    }
    if (_task_container) {
        lv_obj_set_hidden(_task_container, false);
    }
    _refresh_task_display();
}

void ui_task_list_hide(void) {
    if (_keyboard) {
        ui_keyboard_delete(_keyboard);
        _keyboard = NULL;
    }
    if (_task_root_cont) {
        lv_obj_set_hidden(_task_root_cont, true);
    }
    if (_input_cont) {
        lv_obj_set_hidden(_input_cont, true);
    }
    if (_textarea_task) {
        lv_obj_set_hidden(_textarea_task, true);
    }
    if (_btn_add_task) {
        lv_obj_set_hidden(_btn_add_task, true);
    }
    if (_task_container) {
        lv_obj_set_hidden(_task_container, true);
    }
}

void ui_task_list_add_task(const char *task_text) {
    if (_task_count >= MAX_TASKS) {
        ESP_LOGW(TAG, "Task limit reached");
        return;
    }
    
    if (!task_text || strlen(task_text) == 0) {
        return;
    }
    
    strncpy(_tasks[_task_count].text, task_text, MAX_TASK_LEN - 1);
    _tasks[_task_count].text[MAX_TASK_LEN - 1] = '\0';
    _tasks[_task_count].completed = false;
    _tasks[_task_count].created_time = 0; 
    _task_count++;
    
    // Use display lock to prevent race conditions during refresh only
    bsp_display_lock(0);
    _refresh_task_display();
    bsp_display_unlock();
}

void ui_task_list_remove_task(uint16_t index) {
    if (index >= _task_count) {
        return;
    }
    
    bsp_display_lock(0);
    for (uint16_t i = index; i < _task_count - 1; i++) {
        _tasks[i] = _tasks[i + 1];
    }
    _task_count--;
    _refresh_task_display();
    bsp_display_unlock();
}

uint16_t ui_task_list_get_count(void) {
    return _task_count;
}

lv_obj_t *ui_task_list_get_input_obj(void) {
    return _textarea_task;
}
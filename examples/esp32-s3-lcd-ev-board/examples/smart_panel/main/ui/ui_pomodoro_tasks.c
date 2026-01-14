#include "ui_pomodoro_tasks.h"
#include "ui_pomodoro_analytics.h"
#include "ui_main.h"
#include "esp_log.h"

static lv_obj_t *_textarea_sessions = NULL;
static lv_obj_t *_input_cont = NULL;
static lv_obj_t *_right_panel = NULL;
static void _textarea_sessions_focus_cb(lv_obj_t *obj, lv_event_t event);
static void _textarea_focus_cb(lv_obj_t *obj, lv_event_t event);
static void _btn_add_task_cb(lv_obj_t *obj, lv_event_t event); //tasks
static void _checkbox_event_cb(lv_obj_t *obj, lv_event_t event); //checkboxes
lv_obj_t *_textarea_task = NULL;
lv_obj_t *_btn_add_task = NULL;
lv_obj_t *_task_list = NULL;
lv_obj_t *_keyboard = NULL;
lv_obj_t *_btn_analytics = NULL;
static void _btn_analytics_cb(lv_obj_t *obj, lv_event_t event);


// Initialize task management UI
void ui_pomodoro_tasks_init(lv_obj_t *parent_cont, lv_obj_t *page_pomodoro)
{
    // Don't reinitialize if already created
    if (_input_cont != NULL) {
        return;
    }
    
    // Store the right panel reference
    _right_panel = parent_cont;
    
    // task input cont
    _input_cont = lv_cont_create(_right_panel, NULL);
    lv_obj_set_size(_input_cont, lv_obj_get_width(_right_panel) - 20, 50);
    lv_cont_set_layout(_input_cont, LV_LAYOUT_ROW_MID);
    lv_obj_set_style_local_pad_inner(_input_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 5);
    lv_obj_set_style_local_border_width(_input_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_bg_opa(_input_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);

    // task input field
    _textarea_task = lv_textarea_create(_input_cont, NULL);
    lv_obj_set_size(_textarea_task, lv_obj_get_width(_right_panel) - 130, 40);
    lv_textarea_set_placeholder_text(_textarea_task, "Enter task...");
    lv_textarea_set_text(_textarea_task, "");
    lv_obj_set_event_cb(_textarea_task, _textarea_focus_cb);

    // add task button
    _btn_add_task = lv_btn_create(_input_cont, NULL);
    lv_obj_set_size(_btn_add_task, 80, 40);
    lv_obj_set_event_cb(_btn_add_task, _btn_add_task_cb);
    lv_obj_set_style_local_value_str(_btn_add_task, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, "Add");

    // task list cont
    // need to store in cloud/database - persist task list and retrieve on session start
    _task_list = lv_list_create(_right_panel, NULL);
    lv_obj_set_size(_task_list, lv_obj_get_width(_right_panel)-20, lv_obj_get_height(_right_panel)-120); // fits input field
    lv_obj_set_drag_dir(_task_list, LV_DRAG_DIR_VER);
    lv_list_set_scrollbar_mode(_task_list, LV_SCROLLBAR_MODE_AUTO); 
    lv_obj_set_style_local_pad_all(_task_list, LV_LIST_PART_SCROLLABLE, LV_STATE_DEFAULT, 5);  

    // analytics button on right_panel
    _btn_analytics = lv_btn_create(_right_panel, NULL);
    lv_obj_set_size(_btn_analytics, 70, 20);
    lv_obj_set_style_local_radius(_btn_analytics, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, 8);
    lv_obj_set_style_local_bg_color(_btn_analytics, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(180, 167, 214));
    lv_obj_t *label = lv_label_create(_btn_analytics, NULL);
    lv_label_set_text(label, "Stats");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);

    // event callback
    lv_obj_set_event_cb(_btn_analytics, _btn_analytics_cb);
}

// Cleanup task management UI
void ui_pomodoro_tasks_cleanup(void)
{
    // Just clear references - don't delete to avoid issues with event callbacks
    // Objects will be recreated on next init
    _textarea_task = NULL;
    _btn_add_task = NULL;
    _btn_analytics = NULL;
    _input_cont = NULL;
    _task_list = NULL;
    _right_panel = NULL;
}

//add task button 
static void _btn_add_task_cb(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        // need to store in cloud/database - save task to list
        const char *task_text = lv_textarea_get_text(_textarea_task);
        if (strlen(task_text) == 0) return;

        // get scrollable part of the list
        lv_obj_t *list_page = lv_page_get_scrollable(_task_list);

        // checkbox + task text cont
        lv_obj_t *task_cont = lv_cont_create(_task_list, NULL);
        lv_obj_set_size(task_cont, lv_obj_get_width(_task_list) - 40, 40);
        lv_cont_set_layout(task_cont, LV_LAYOUT_ROW_MID);
        lv_obj_set_style_local_pad_inner(task_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 10);
        lv_obj_set_style_local_border_width(task_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 1);
        lv_obj_set_style_local_border_color(task_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(200, 200, 200));
        lv_obj_set_style_local_radius(task_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 5);

        // add checkbox
        lv_obj_t *checkbox = lv_checkbox_create(task_cont, NULL);
        lv_checkbox_set_text(checkbox, task_text);
        lv_obj_set_event_cb(checkbox, _checkbox_event_cb);

        // clear input field
        lv_textarea_set_text(_textarea_task, "");
    }
}

//checkbox 
static void _checkbox_event_cb(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        if (lv_checkbox_is_checked(obj))
        {
            // need to update in cloud/database - remove task from list
            // get parent cont, delete it (removes entire task)
            lv_obj_t *task_cont = lv_obj_get_parent(obj);
            lv_obj_del(task_cont);
        }
    }
}

//keyboard for task input
static void _textarea_focus_cb(lv_obj_t *obj, lv_event_t event)
{
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

//analytics button
static void _btn_analytics_cb(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_CLICKED)
    {
        // Only show analytics - hide pomodoro through ui_page mechanism
        ui_pomodoro_analytics_show(NULL);
    }
}

// Show tasks UI elements
void ui_pomodoro_tasks_show(void)
{
    if (_textarea_task) lv_obj_set_hidden(_textarea_task, false);
    if (_btn_add_task) lv_obj_set_hidden(_btn_add_task, false);
    if (_task_list) lv_obj_set_hidden(_task_list, false);
    if (_btn_analytics) lv_obj_set_hidden(_btn_analytics, false);
}

// Hide tasks UI elements
void ui_pomodoro_tasks_hide(void)
{
    if (_textarea_task) lv_obj_set_hidden(_textarea_task, true);
    if (_btn_add_task) lv_obj_set_hidden(_btn_add_task, true);
    if (_task_list) lv_obj_set_hidden(_task_list, true);
    if (_btn_analytics) lv_obj_set_hidden(_btn_analytics, true);
}

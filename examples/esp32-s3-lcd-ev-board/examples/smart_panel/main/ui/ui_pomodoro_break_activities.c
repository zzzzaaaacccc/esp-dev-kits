#include "ui_pomodoro_break_activities.h"
#include "ui_pomodoro_task_list.h"
#include "esp_log.h"
#include "lvgl.h"
#include "ui_font_declare.h"

static const char *TAG = "UI_BREAK_ACTIVITIES";

static lv_obj_t *_right_panel = NULL;
static lv_obj_t *_break_main_menu_cont = NULL; 
static lv_obj_t *_break_content_cont = NULL; 
static lv_obj_t *_exercise_btn = NULL;
static lv_obj_t *_nature_btn = NULL;
static lv_obj_t *_mindfulness_btn = NULL;
static lv_obj_t *_relaxation_btn = NULL;
static break_activity_t _selected_activity = ACTIVITY_NONE;

/* Forward declarations*/
static void _exercise_btn_cb(lv_obj_t *obj, lv_event_t event);
static void _nature_btn_cb(lv_obj_t *obj, lv_event_t event);
static void _mindfulness_btn_cb(lv_obj_t *obj, lv_event_t event);
static void _relaxation_btn_cb(lv_obj_t *obj, lv_event_t event);
static void _clear_break_content(void);
static void _show_break_menu(void);

static void _exercise_btn_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        _selected_activity = ACTIVITY_PHYSICAL_EXERCISE;
        _clear_break_content();
        if (_break_main_menu_cont) {
            lv_obj_set_hidden(_break_main_menu_cont, true);
        }
        if (_break_content_cont) {
            lv_obj_set_hidden(_break_content_cont, false);
        }
    }
}

static void _nature_btn_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        _selected_activity = ACTIVITY_NATURE_BREAK;
        _clear_break_content();
        if (_break_main_menu_cont) {
            lv_obj_set_hidden(_break_main_menu_cont, true);
        }
        if (_break_content_cont) {
            lv_obj_set_hidden(_break_content_cont, false);
        }
    }
}

static void _mindfulness_btn_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        _selected_activity = ACTIVITY_MINDFULNESS;
        _clear_break_content();
        if (_break_main_menu_cont) {
            lv_obj_set_hidden(_break_main_menu_cont, true);
        }
        if (_break_content_cont) {
            lv_obj_set_hidden(_break_content_cont, false);
        }
    }
}

static void _relaxation_btn_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        _selected_activity = ACTIVITY_RELAXATION;
        _clear_break_content();
        if (_break_main_menu_cont) {
            lv_obj_set_hidden(_break_main_menu_cont, true);
        }
        if (_break_content_cont) {
            lv_obj_set_hidden(_break_content_cont, false);
        }
    }
}

static void _clear_break_content(void) {
    if (_break_content_cont) {
        lv_obj_clean(_break_content_cont);
    }
}

static void _show_break_menu(void) {
    if (_break_main_menu_cont) {
        lv_obj_set_hidden(_break_main_menu_cont, false);
    }
}


void ui_break_activities_init(lv_obj_t *content_area_container) {
    _right_panel = content_area_container;
    
    lv_obj_t *scrl = lv_page_get_scrl(_right_panel);
    int scrl_width = lv_obj_get_width(scrl);
    if (scrl_width <= 0) {
        scrl_width = lv_obj_get_width(_right_panel) - 10;
    }
    
    _break_main_menu_cont = lv_cont_create(scrl, NULL);
    int menu_width = scrl_width - 20;
    if (menu_width < 50) {
        menu_width = scrl_width;
    }
    lv_obj_set_width(_break_main_menu_cont, menu_width);
    lv_obj_align(_break_main_menu_cont, scrl, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_cont_set_fit2(_break_main_menu_cont, LV_FIT_NONE, LV_FIT_TIGHT);
    lv_cont_set_layout(_break_main_menu_cont, LV_LAYOUT_COLUMN_MID);
    lv_obj_set_style_local_bg_opa(_break_main_menu_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
    lv_obj_set_style_local_pad_top(_break_main_menu_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_left(_break_main_menu_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 10);
    lv_obj_set_style_local_pad_right(_break_main_menu_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 10);
    lv_obj_set_style_local_pad_top(_break_main_menu_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 5);
    lv_obj_set_style_local_pad_bottom(_break_main_menu_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 5);
    lv_obj_set_style_local_border_width(_break_main_menu_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_all(_break_main_menu_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_inner(_break_main_menu_cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, 15);

    lv_obj_set_hidden(_break_main_menu_cont, true);
    
    int btn_width = menu_width - 20;
    int btn_height = 50;
    
    _exercise_btn = lv_btn_create(_break_main_menu_cont, NULL);
    lv_obj_set_size(_exercise_btn, btn_width, btn_height);
    lv_obj_set_event_cb(_exercise_btn, _exercise_btn_cb);
    lv_obj_set_drag(_exercise_btn, true);
    lv_obj_set_drag_dir(_exercise_btn, LV_DRAG_DIR_VER);
    lv_obj_set_drag_parent(_exercise_btn, true);
    lv_obj_set_style_local_bg_color(_exercise_btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(100, 200, 150));
    lv_obj_t *exercise_label = lv_label_create(_exercise_btn, NULL);
    lv_label_set_text(exercise_label, "Physical Exercise");
    lv_obj_set_width(exercise_label, btn_width - 16);
    lv_label_set_long_mode(exercise_label, LV_LABEL_LONG_BREAK);
    lv_label_set_align(exercise_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_style_local_text_font(exercise_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_en_18);
    
    _nature_btn = lv_btn_create(_break_main_menu_cont, NULL);
    lv_obj_set_size(_nature_btn, btn_width, btn_height);
    lv_obj_set_event_cb(_nature_btn, _nature_btn_cb);
    lv_obj_set_drag(_nature_btn, true);
    lv_obj_set_drag_dir(_nature_btn, LV_DRAG_DIR_VER);
    lv_obj_set_drag_parent(_nature_btn, true);
    lv_obj_set_style_local_bg_color(_nature_btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(100, 180, 220));
    lv_obj_t *nature_label = lv_label_create(_nature_btn, NULL);
    lv_label_set_text(nature_label, "Nature Break");
    lv_obj_set_width(nature_label, btn_width - 16);
    lv_label_set_long_mode(nature_label, LV_LABEL_LONG_BREAK);
    lv_label_set_align(nature_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_style_local_text_font(nature_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_en_18);
    
    _mindfulness_btn = lv_btn_create(_break_main_menu_cont, NULL);
    lv_obj_set_size(_mindfulness_btn, btn_width, btn_height);
    lv_obj_set_event_cb(_mindfulness_btn, _mindfulness_btn_cb);
    lv_obj_set_drag(_mindfulness_btn, true);
    lv_obj_set_drag_dir(_mindfulness_btn, LV_DRAG_DIR_VER);
    lv_obj_set_drag_parent(_mindfulness_btn, true);
    lv_obj_set_style_local_bg_color(_mindfulness_btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(180, 150, 200));
    lv_obj_t *mindfulness_label = lv_label_create(_mindfulness_btn, NULL);
    lv_label_set_text(mindfulness_label, "Mindfulness");
    lv_obj_set_width(mindfulness_label, btn_width - 16);
    lv_label_set_long_mode(mindfulness_label, LV_LABEL_LONG_BREAK);
    lv_label_set_align(mindfulness_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_style_local_text_font(mindfulness_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_en_18);
    
    _relaxation_btn = lv_btn_create(_break_main_menu_cont, NULL);
    lv_obj_set_size(_relaxation_btn, btn_width, btn_height);
    lv_obj_set_event_cb(_relaxation_btn, _relaxation_btn_cb);
    lv_obj_set_drag(_relaxation_btn, true);
    lv_obj_set_drag_dir(_relaxation_btn, LV_DRAG_DIR_VER);
    lv_obj_set_drag_parent(_relaxation_btn, true);
    lv_obj_set_style_local_bg_color(_relaxation_btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(220, 180, 150));
    lv_obj_t *relaxation_label = lv_label_create(_relaxation_btn, NULL);
    lv_label_set_text(relaxation_label, "Relaxation");
    lv_obj_set_width(relaxation_label, btn_width - 16);
    lv_label_set_long_mode(relaxation_label, LV_LABEL_LONG_BREAK);
    lv_label_set_align(relaxation_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_style_local_text_font(relaxation_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_en_18);
    
    _break_content_cont = lv_cont_create(scrl, NULL);
    lv_obj_set_width(_break_content_cont, menu_width);
    lv_obj_align(_break_content_cont, scrl, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_cont_set_fit2(_break_content_cont, LV_FIT_NONE, LV_FIT_TIGHT);
    lv_cont_set_layout(_break_content_cont, LV_LAYOUT_COLUMN_MID);
    lv_obj_set_style_local_border_width(_break_content_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_bg_opa(_break_content_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
    lv_obj_set_hidden(_break_content_cont, true);
}

void ui_break_activities_show_menu(void) {
    if (_break_main_menu_cont) {
        lv_obj_set_hidden(_break_main_menu_cont, false);
        // Keep background transparent; just make it clickable
        lv_obj_set_style_local_bg_opa(_break_main_menu_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
        lv_obj_set_click(_break_main_menu_cont, true);
    }
}

void ui_break_activities_hide_menu(void) {
    if (_break_main_menu_cont) {
        lv_obj_set_hidden(_break_main_menu_cont, true);
    }
    
    if (_break_content_cont) {
        lv_obj_set_hidden(_break_content_cont, true);
        lv_obj_set_style_local_bg_opa(_break_content_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
        lv_obj_set_click(_break_content_cont, false);
    }
    _clear_break_content();
}

void ui_break_activities_show_task_list(void) {
    if (_break_main_menu_cont) {
        lv_obj_set_hidden(_break_main_menu_cont, true);
    }
    if (_break_content_cont) {
        lv_obj_set_hidden(_break_content_cont, true);
    }
}

void ui_break_activities_start_activity(break_activity_t activity) {
    if (_break_content_cont) {
        lv_obj_set_hidden(_break_content_cont, false);
    }
    _clear_break_content();
}

break_activity_t ui_break_activities_get_selected(void) {
    return _selected_activity;
}

void ui_break_activities_exit(void) {
    if (!_right_panel) {
        return;
    }
    if (_break_main_menu_cont) {
        lv_obj_set_hidden(_break_main_menu_cont, true);
    }
    if (_break_content_cont) {
        lv_obj_set_hidden(_break_content_cont, true);
    }
    _clear_break_content();
}

void ui_break_create_view(void) {
    ui_task_list_show();
}

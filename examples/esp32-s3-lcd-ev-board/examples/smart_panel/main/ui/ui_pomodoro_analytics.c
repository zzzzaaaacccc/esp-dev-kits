
#include "ui_main.h"
#include "lvgl.h"
#include "ui_pomodoro_analytics.h"

/* UI function declaration */
ui_func_desc_t ui_pomodoro_analytics_func = {
    .name = "Pomodoro Analytics",
    .init = ui_pomodoro_analytics_init,
    .show = ui_pomodoro_analytics_show,
    .hide = ui_pomodoro_analytics_hide,
};

static ui_state_t ui_pomodoro_analytics_state = ui_state_dis;

static lv_obj_t *_analytics_cont = NULL;
static lv_obj_t *_label_weekly_title = NULL;
static lv_obj_t *_sessions_title_label = NULL;
static lv_obj_t *_sessions_value_label = NULL;
static lv_obj_t *_time_title_label = NULL;
static lv_obj_t *_time_value_label = NULL;
static lv_obj_t *_chart_focus_time = NULL;
static lv_chart_series_t *_focus_time_series = NULL;
static lv_task_t *_anim_lv_task = NULL;
static int _anim_point_index = 0;

//need get from cloud/database -> total_sessions_week
static uint16_t total_sessions_week = 24;
//need get from cloud/database -> total_time_minutes_week
static uint16_t total_time_minutes_week = 555;

// sample data
static uint16_t focus_time_data[] = { 25, 50, 75, 100, 50, 75, 180 }; 

static void _anim_chart_lv_task(lv_task_t *task);
static void _prepare_chart_and_labels(void);


void ui_pomodoro_analytics_init_in_panel(lv_obj_t *content_area) {
    
    // Get scrollable width from the page
    lv_obj_t *scrl = lv_page_get_scrl(content_area);
    int scrl_width = lv_obj_get_width(scrl);
    if (scrl_width <= 0) {
        scrl_width = lv_obj_get_width(content_area) - 10;
    }
    
    _analytics_cont = lv_cont_create(scrl, NULL);
    int cont_width = scrl_width - 20;
    if (cont_width < 50) {
        cont_width = scrl_width;
    }
    lv_obj_set_width(_analytics_cont, cont_width);
    lv_obj_align(_analytics_cont, scrl, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_obj_set_style_local_bg_opa(_analytics_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_width(_analytics_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_cont_set_layout(_analytics_cont, LV_LAYOUT_COLUMN_MID);
    // Keep fixed width (with side margins) and auto-height, without forcing full-page width
    lv_cont_set_fit2(_analytics_cont, LV_FIT_NONE, LV_FIT_TIGHT);
    lv_obj_set_style_local_pad_top(_analytics_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_all(_analytics_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 5);
    lv_obj_set_style_local_pad_inner(_analytics_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 5);

    _label_weekly_title = lv_label_create(_analytics_cont, NULL);
    lv_label_set_text(_label_weekly_title, "Weekly Insights");
    lv_obj_set_style_local_text_font(_label_weekly_title, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_en_bold_20);
    lv_label_set_align(_label_weekly_title, LV_LABEL_ALIGN_CENTER);

    lv_obj_t *stats_cont = lv_cont_create(_analytics_cont, NULL);
    lv_obj_set_width(stats_cont, lv_obj_get_width(_analytics_cont) - 10);
    // Center content to avoid left clipping; labels themselves will wrap
    lv_cont_set_layout(stats_cont, LV_LAYOUT_COLUMN_MID);
    lv_cont_set_fit2(stats_cont, LV_FIT_NONE, LV_FIT_TIGHT);
    lv_obj_set_style_local_bg_opa(stats_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_width(stats_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_all(stats_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_inner(stats_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 8);


    _sessions_title_label = lv_label_create(stats_cont, NULL);
    lv_label_set_text(_sessions_title_label, "Total Sessions:");
    lv_obj_set_width(_sessions_title_label, lv_obj_get_width(stats_cont));
    lv_label_set_long_mode(_sessions_title_label, LV_LABEL_LONG_CROP);
    lv_label_set_align(_sessions_title_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_style_local_text_font(_sessions_title_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_en_18);

    _sessions_value_label = lv_label_create(stats_cont, NULL);
    lv_label_set_text(_sessions_value_label, "24");
    lv_obj_set_width(_sessions_value_label, lv_obj_get_width(stats_cont));
    lv_label_set_long_mode(_sessions_value_label, LV_LABEL_LONG_EXPAND);
    lv_label_set_align(_sessions_value_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_style_local_text_font(_sessions_value_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_en_24);
    lv_obj_set_style_local_text_color(_sessions_value_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(0, 120, 220));

    _time_title_label = lv_label_create(stats_cont, NULL);
    lv_label_set_text(_time_title_label, "Total focus time:");
    lv_obj_set_width(_time_title_label, lv_obj_get_width(stats_cont));
    lv_label_set_long_mode(_time_title_label, LV_LABEL_LONG_CROP);
    lv_label_set_align(_time_title_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_style_local_text_font(_time_title_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_en_18);

    _time_value_label = lv_label_create(stats_cont, NULL);
    lv_label_set_text(_time_value_label, "555 minutes (9h15min)");
    lv_obj_set_width(_time_value_label, lv_obj_get_width(stats_cont));
    lv_label_set_long_mode(_time_value_label, LV_LABEL_LONG_EXPAND);
    lv_label_set_align(_time_value_label, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_style_local_text_font(_time_value_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_en_18);
    lv_obj_set_style_local_text_color(_time_value_label, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(0, 120, 220));

    _chart_focus_time = lv_chart_create(_analytics_cont, NULL);
    lv_obj_set_size(_chart_focus_time, lv_obj_get_width(_analytics_cont) - 40, 160);
    lv_obj_set_style_local_margin_top(_chart_focus_time, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 50);
    lv_chart_set_range(_chart_focus_time, 0, 200);
    lv_chart_set_point_count(_chart_focus_time, 7);
    lv_obj_set_style_local_border_width(_chart_focus_time, LV_CHART_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_set_style_local_pad_left(_chart_focus_time, LV_CHART_PART_BG, LV_STATE_DEFAULT, 55);
    lv_obj_set_style_local_pad_bottom(_chart_focus_time, LV_CHART_PART_BG, LV_STATE_DEFAULT, 30);
    lv_obj_set_style_local_pad_right(_chart_focus_time, LV_CHART_PART_BG, LV_STATE_DEFAULT, 10);
    lv_obj_set_style_local_pad_top(_chart_focus_time, LV_CHART_PART_BG, LV_STATE_DEFAULT, 10);
    lv_obj_set_style_local_text_font(_chart_focus_time, LV_CHART_PART_BG, LV_STATE_DEFAULT, &font_en_18);
    lv_obj_set_style_local_text_color(_chart_focus_time, LV_CHART_PART_BG, LV_STATE_DEFAULT, LV_COLOR_MAKE(100, 100, 100));
    lv_chart_set_div_line_count(_chart_focus_time, 0, 0);

    lv_chart_set_x_tick_texts(_chart_focus_time, "M\nT\nW\nT\nF\nS\nS", 2, LV_CHART_AXIS_DRAW_LAST_TICK | LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_y_tick_texts(_chart_focus_time, "0\n50\n100\n150\n200", 2, LV_CHART_AXIS_PRIMARY_Y | LV_CHART_AXIS_INVERSE_LABELS_ORDER);
    lv_chart_set_x_tick_length(_chart_focus_time, 2, 2);
    lv_chart_set_y_tick_length(_chart_focus_time, 2, 2);

    lv_obj_set_hidden(_analytics_cont, true);
    
    ui_pomodoro_analytics_state = ui_state_show;
}

void ui_pomodoro_analytics_init(void *data) {
    ui_pomodoro_analytics_state = ui_state_dis;
}

void ui_pomodoro_analytics_show(void *data) {
    if (_analytics_cont) {
        ui_pomodoro_analytics_state = ui_state_show;
        lv_obj_set_hidden(_analytics_cont, false);
        lv_obj_set_style_local_bg_opa(_analytics_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
        lv_obj_set_click(_analytics_cont, true);

        _prepare_chart_and_labels();

        if (_anim_lv_task) {
            lv_task_del(_anim_lv_task);
            _anim_lv_task = NULL;
        }
        _anim_point_index = 0;
        _anim_lv_task = lv_task_create(_anim_chart_lv_task, 200, LV_TASK_PRIO_MID, NULL);
    }
}

void ui_pomodoro_analytics_hide(void *data) {
    ui_pomodoro_analytics_state = ui_state_hide;

    if (_anim_lv_task) {
        lv_task_del(_anim_lv_task);
        _anim_lv_task = NULL;
    }
    
    if (_focus_time_series) {
        if (_chart_focus_time) {
            lv_chart_clear_series(_chart_focus_time, _focus_time_series);
        }
        _focus_time_series = NULL;
    }
    if (_analytics_cont) {
        lv_obj_set_hidden(_analytics_cont, true);
        lv_obj_set_style_local_bg_opa(_analytics_cont, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
    }
}

static void _btn_back_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        ui_pomodoro_analytics_hide(NULL);
        ui_pomodoro_timer_show(NULL);
    }
}

static void _prepare_chart_and_labels(void) {
    if (!_chart_focus_time || !_sessions_value_label || !_time_value_label) {
        return;
    }

    // Ensure label widths are correct even if the page/containers were not laid out at init time
    lv_obj_t *stats_cont = lv_obj_get_parent(_sessions_value_label);
    lv_coord_t stats_w = stats_cont ? lv_obj_get_width(stats_cont) : 0;
    if (stats_w <= 0 && _analytics_cont) {
        stats_w = lv_obj_get_width(_analytics_cont) - 10;
    }
    if (stats_w > 0) {
        if (_sessions_title_label) lv_obj_set_width(_sessions_title_label, stats_w);
        if (_sessions_value_label) lv_obj_set_width(_sessions_value_label, stats_w);
        if (_time_title_label) lv_obj_set_width(_time_title_label, stats_w);
        if (_time_value_label) lv_obj_set_width(_time_value_label, stats_w);
    }

    if (_focus_time_series) {
        lv_chart_clear_series(_chart_focus_time, _focus_time_series);
        _focus_time_series = NULL;
    }

    _focus_time_series = lv_chart_add_series(_chart_focus_time, LV_COLOR_MAKE(100, 150, 200));
    lv_chart_set_type(_chart_focus_time, LV_CHART_TYPE_LINE);
    lv_obj_set_style_local_line_width(_chart_focus_time, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 3);
    lv_obj_set_style_local_size(_chart_focus_time, LV_CHART_PART_SERIES, LV_STATE_DEFAULT, 6);

    for (int j = 0; j < 7; j++) {
        lv_chart_set_next(_chart_focus_time, _focus_time_series, 0);
    }

    char buf[80];
    snprintf(buf, sizeof(buf), "%u", total_sessions_week);
    lv_label_set_text(_sessions_value_label, buf);

    uint16_t hours = total_time_minutes_week / 60;
    uint16_t mins = total_time_minutes_week % 60;
    if (hours > 0) {
        snprintf(buf, sizeof(buf), "%u minutes (%uh%02umin)", total_time_minutes_week, hours, mins);
    } else {
        snprintf(buf, sizeof(buf), "%u minutes", total_time_minutes_week);
    }
    lv_label_set_text(_time_value_label, buf);
}

static void _anim_chart_lv_task(lv_task_t *task) {
    (void)task;
    if (ui_pomodoro_analytics_state == ui_state_hide) {
        return;
    }
    if (!_chart_focus_time || !_focus_time_series) {
        return;
    }

    if (_anim_point_index < 7) {
        lv_chart_set_point_id(_chart_focus_time, _focus_time_series, focus_time_data[_anim_point_index], _anim_point_index);
        lv_chart_refresh(_chart_focus_time);
        _anim_point_index++;
    } else {
        if (_anim_lv_task) {
            lv_task_del(_anim_lv_task);
            _anim_lv_task = NULL;
        }
    }
}

void ui_pomodoro_analytics_update_sessions(uint16_t sessions) {
    total_sessions_week = sessions;
    if (_sessions_value_label) {
        bsp_display_lock(0);
        char buf[64];
        snprintf(buf, sizeof(buf), "%u", sessions);
        lv_label_set_text(_sessions_value_label, buf);
        bsp_display_unlock();
    }
}

void ui_pomodoro_analytics_update_time(uint16_t minutes) {
    total_time_minutes_week = minutes;
    if (_time_value_label) {
        bsp_display_lock(0);
        char buf[64];
        uint16_t hours = minutes / 60;
        uint16_t mins = minutes % 60;
        if (hours > 0) {
            snprintf(buf, sizeof(buf), "%u minutes (%uh%02umin)", minutes, hours, mins);
        } else {
            snprintf(buf, sizeof(buf), "%u minutes", minutes);
        }
        lv_label_set_text(_time_value_label, buf);
        bsp_display_unlock();
    }
}
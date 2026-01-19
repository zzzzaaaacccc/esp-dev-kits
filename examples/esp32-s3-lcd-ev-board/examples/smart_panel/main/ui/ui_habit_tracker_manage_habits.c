#include "ui_habit_tracker_manage_habits.h"
#include "ui_main.h"
#include "ui_keyboard.h"
#include "ui_habit_tracker_today_view.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>

// habit storage
#define INITIAL_HABITS_CAPACITY 10
const uint8_t morn_hr = 8;
const uint8_t morn_min = 0;
const uint8_t afternoon_hr = 14;
const uint8_t afternoon_min = 0;
const uint8_t night_hr = 21;
const uint8_t night_min = 0;

static habit_t *habits = NULL;
static uint16_t habit_count = 0;
static uint16_t habits_capacity = 0;
static int16_t selected_habit_idx = -1; // -1: new habit

// lvgl objects
static lv_obj_t *list_container = NULL;
static lv_obj_t *habit_list = NULL;
static lv_obj_t *edit_panel = NULL;
static lv_obj_t *ta_habit_name = NULL;
static lv_obj_t *ta_habit_description = NULL;
static lv_obj_t *dd_frequency = NULL;
static lv_obj_t *frequency_container = NULL;
static lv_obj_t *days_container = NULL;
static lv_obj_t *day_buttons[7] = {NULL};
static lv_obj_t *label_days = NULL;
static lv_obj_t *label_selected_dates = NULL;
static lv_obj_t *dd_time_option = NULL;
static lv_obj_t *time_container = NULL;
static lv_obj_t *roller_hour = NULL;
static lv_obj_t *roller_minute = NULL;
static lv_obj_t *label_hour = NULL;
static lv_obj_t *label_minute = NULL;
static lv_obj_t *label_preset_time = NULL;
static lv_obj_t *btn_save = NULL;
static lv_obj_t *btn_give_up = NULL;
static lv_obj_t *btn_add_new = NULL;
static lv_obj_t *_keyboard = NULL;
static lv_obj_t *calendar = NULL;
static lv_obj_t *calendar_popup = NULL;

// day selection state 
static bool day_selected[7] = {false};

// day labels 
static const char *day_labels[] = {"M", "T", "W", "T", "F", "S", "S"};

/* Static function forward declaration */
static void refresh_habit_list(void);
static void show_edit_panel(int16_t habit_idx);
static void hide_edit_panel(void);
static void save_current_habit(void);
static void frequency_option_event_cb(lv_obj_t *obj, lv_event_t event);
static void calendar_close_event_cb(lv_obj_t *obj, lv_event_t event);
static void calendar_event_cb(lv_obj_t *obj, lv_event_t event);
static bool ensure_habits_capacity(void);
static void update_selected_dates_label(void);

// calendar
#define MAX_CUSTOM_DATES 31
static lv_calendar_date_t selected_custom_dates[MAX_CUSTOM_DATES];
static uint16_t selected_custom_dates_count = 0;

// update selected dates display
static void update_selected_dates_label(void) {
    static char date_str[256];
    strcpy(date_str, "Selected dates: ");
    
    if (selected_custom_dates_count == 0) {
        strcat(date_str, "None");
        lv_label_set_text(label_selected_dates, date_str);
        return;
    }
    
    // sort chronologically
    for (int i = 0; i < selected_custom_dates_count - 1; i++) {
        for (int j = i + 1; j < selected_custom_dates_count; j++) {
            lv_calendar_date_t *a = &selected_custom_dates[i];
            lv_calendar_date_t *b = &selected_custom_dates[j];
            
            int a_val = a->year * 10000 + a->month * 100 + a->day;
            int b_val = b->year * 10000 + b->month * 100 + b->day;
            
            if (a_val > b_val) {
                lv_calendar_date_t temp = *a;
                *a = *b;
                *b = temp;
            }
        }
    }
    
    const char *months[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    for (int i = 0; i < selected_custom_dates_count; i++) {
        char date_buf[20];
        snprintf(date_buf, sizeof(date_buf), "%d %s %d", 
            selected_custom_dates[i].day,
            months[selected_custom_dates[i].month],
            selected_custom_dates[i].year);
        
        if (i > 0) strcat(date_str, ", ");
        strcat(date_str, date_buf);
    }
    
    lv_label_set_text(label_selected_dates, date_str);
}

static void update_preset_time_label(uint16_t time_option) {
    if (time_option == 0) {
        lv_obj_set_hidden(label_preset_time, true);
    } else if (time_option == 1) {
        lv_obj_set_hidden(label_preset_time, true);
    } else if (time_option == 2) {
        lv_label_set_text(label_preset_time, "Morning (8:00 AM)");
        lv_obj_set_hidden(label_preset_time, false);
    } else if (time_option == 3) {
        lv_label_set_text(label_preset_time, "Afternoon (12:00 PM)");
        lv_obj_set_hidden(label_preset_time, false);
    } else if (time_option == 4) {
        lv_label_set_text(label_preset_time, "Night (8:00 PM)");
        lv_obj_set_hidden(label_preset_time, false);
    }
}

static void habit_list_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        // get the actual habit index stored in the button's user data (cloud/database)
        uint16_t habit_idx = (uint16_t)(uintptr_t)lv_obj_get_user_data(obj);
        if (habit_idx < habit_count) {
            show_edit_panel(habit_idx);
        }
    }
}

static void add_new_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        show_edit_panel(-1);
    }
}

static void day_btn_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        for (int i = 0; i < 7; i++) {
            if (obj == day_buttons[i]) {
                day_selected[i] = !day_selected[i];
                if (day_selected[i]) {
                    lv_obj_set_style_local_bg_color(obj, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_ORANGE);
                } else {
                    lv_obj_set_style_local_bg_color(obj, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
                }
                break;
            }
        }
    }
}

static void save_btn_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        save_current_habit();
        hide_edit_panel();
        refresh_habit_list();
        ui_today_view_refresh();
    }
}

static void give_up_btn_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        if (selected_habit_idx >= 0 && selected_habit_idx < habit_count) {
            // need to store in cloud/database -> mark habit as inactive/deleted and remove from user's habit list
            habits[selected_habit_idx].is_active = 0;
        }
        hide_edit_panel();
        refresh_habit_list();
        ui_today_view_refresh();
    }
}

static void cancel_edit_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        hide_edit_panel();
    }
}

static void ta_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_FOCUSED || event == LV_EVENT_CLICKED) {
        if (!_keyboard) {
            if (edit_panel) {
                lv_obj_align(edit_panel, NULL, LV_ALIGN_CENTER, 0, -80);
            }
            _keyboard = ui_keyboard_create(obj);
        }
    }
    else if (event == LV_EVENT_DEFOCUSED || event == LV_EVENT_APPLY) {
        if (_keyboard) {
            ui_keyboard_delete(_keyboard);
            _keyboard = NULL;
            if (edit_panel) {
                lv_obj_align(edit_panel, NULL, LV_ALIGN_CENTER, 0, 0);
            }
        }
    }
}

static void time_option_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_VALUE_CHANGED) {
        uint16_t selected = lv_dropdown_get_selected(obj);
        
        if (selected == 0) {  // no time
            lv_obj_set_hidden(time_container, true);
            lv_obj_set_hidden(label_preset_time, true);
        }
        else if (selected == 1) {  // specific time
            lv_obj_set_hidden(time_container, false);
            lv_obj_set_size(time_container, 250, 120); 
            lv_obj_set_hidden(label_preset_time, true);
            lv_obj_set_hidden(label_hour, false);
            lv_obj_set_hidden(label_minute, false);
            lv_obj_set_hidden(roller_hour, false);
            lv_obj_set_hidden(roller_minute, false);
        }
        else {  // morn (2), afternoon (3), night (4)
            lv_obj_set_hidden(time_container, true);
            lv_obj_set_hidden(label_hour, true);
            lv_obj_set_hidden(label_minute, true);
            lv_obj_set_hidden(roller_hour, true);
            lv_obj_set_hidden(roller_minute, true);
            update_preset_time_label(selected);
        }
        lv_obj_t *scrl = lv_page_get_scrl(edit_panel);
        lv_obj_invalidate(scrl);
    }
}

static void frequency_option_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_VALUE_CHANGED) {
        uint16_t option = lv_dropdown_get_selected(obj);
        
        if (option == 1) {  // custom
            lv_obj_set_hidden(label_days, true);
            if (days_container) {
                lv_obj_set_hidden(days_container, true);
            }
            lv_obj_set_hidden(frequency_container, false);
            update_selected_dates_label();
        } else {  // weekly
            lv_label_set_text(label_days, "On days:");
            lv_obj_set_hidden(label_days, false);
            if (days_container) {
                lv_obj_set_hidden(days_container, false);
            }
            lv_obj_set_hidden(frequency_container, true);
        }
        lv_obj_t *scrl = lv_page_get_scrl(edit_panel);
        lv_obj_invalidate(scrl);
    }
}

static void calendar_close_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        if (calendar_popup) {
            lv_obj_set_hidden(calendar_popup, true);
            update_selected_dates_label();
        }
    }
}

static void calendar_event_cb(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_VALUE_CHANGED) {
        lv_calendar_date_t *date = lv_calendar_get_pressed_date(obj);
        if (date) {
            // check if date alr selected
            bool found = false;
            int found_idx = -1;
            
            for (int i = 0; i < selected_custom_dates_count; i++) {
                if (selected_custom_dates[i].year == date->year &&
                    selected_custom_dates[i].month == date->month &&
                    selected_custom_dates[i].day == date->day) {
                    found = true;
                    found_idx = i;
                    break;
                }
            }
            
            if (found) {
                // remove date
                for (int i = found_idx; i < selected_custom_dates_count - 1; i++) {
                    selected_custom_dates[i] = selected_custom_dates[i + 1];
                }
                selected_custom_dates_count--;
            } else {
                // add date
                if (selected_custom_dates_count < MAX_CUSTOM_DATES) {
                    selected_custom_dates[selected_custom_dates_count] = *date;
                    selected_custom_dates_count++;
                }
            }
            if (selected_custom_dates_count > 0) {
                lv_calendar_set_highlighted_dates(calendar, selected_custom_dates, selected_custom_dates_count);
            } else {
                lv_calendar_set_highlighted_dates(calendar, NULL, 0);
            }
            update_selected_dates_label();
        }
    }
}

// edit panel UI
static void ui_manage_habits_create_edit_panel(void) {
    edit_panel = lv_page_create(lv_scr_act(), NULL);
    lv_obj_set_size(edit_panel, 500, 460);
    lv_obj_align(edit_panel, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_hidden(edit_panel, true);
    lv_page_set_scrollbar_mode(edit_panel, LV_SCRLBAR_MODE_AUTO);
    lv_page_set_edge_flash(edit_panel, true);
    lv_obj_set_style_local_border_width(edit_panel, LV_PAGE_PART_BG, LV_STATE_DEFAULT, 0);
    lv_obj_t *scrl = lv_page_get_scrl(edit_panel);
    lv_page_set_scrl_layout(edit_panel, LV_LAYOUT_COLUMN_MID);
    lv_obj_set_style_local_pad_inner(scrl, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 8);
    lv_obj_set_style_local_pad_top(scrl, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 5);
    lv_obj_set_style_local_pad_bottom(scrl, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 5);
    
    lv_obj_t *edit_title = lv_label_create(edit_panel, NULL);
    lv_label_set_text(edit_title, "Edit Habit");
    lv_obj_set_style_local_text_font(edit_title, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, &font_en_bold_24);
    lv_label_set_align(edit_title, LV_LABEL_ALIGN_CENTER);
    lv_obj_set_width(edit_title, lv_obj_get_width(edit_panel));
    
    lv_obj_t *label_name = lv_label_create(scrl, NULL);
    lv_label_set_text(label_name, "Habit name:");
    
    ta_habit_name = lv_textarea_create(scrl, NULL);
    lv_obj_set_size(ta_habit_name, 380, 50);
    lv_textarea_set_placeholder_text(ta_habit_name, "Enter habit name");
    lv_textarea_set_one_line(ta_habit_name, false);
    lv_textarea_set_text_sel(ta_habit_name, false);
    lv_obj_set_event_cb(ta_habit_name, ta_event_cb);
    
    lv_obj_t *label_desc = lv_label_create(scrl, NULL);
    lv_label_set_text(label_desc, "Description (optional):");
    
    ta_habit_description = lv_textarea_create(scrl, NULL);
    lv_obj_set_size(ta_habit_description, 380, 50);
    lv_textarea_set_placeholder_text(ta_habit_description, "Add a description");
    lv_textarea_set_one_line(ta_habit_description, false);
    lv_textarea_set_text_sel(ta_habit_description, false);
    lv_obj_set_event_cb(ta_habit_description, ta_event_cb);
    
    lv_obj_t *label_freq = lv_label_create(scrl, NULL);
    lv_label_set_text(label_freq, "Frequency:");
    
    dd_frequency = lv_dropdown_create(scrl, NULL);
    lv_dropdown_set_options(dd_frequency, "Weekly\nCustom");
    lv_obj_set_size(dd_frequency, 180, 35);
    lv_obj_set_event_cb(dd_frequency, frequency_option_event_cb);
    
    label_days = lv_label_create(scrl, NULL);
    lv_label_set_text(label_days, "On days:");
    
    days_container = lv_obj_create(scrl, NULL);
    lv_obj_set_size(days_container, 380, 50);
    lv_obj_set_style_local_bg_opa(days_container, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_width(days_container, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    
    for (int i = 0; i < 7; i++) {
        day_buttons[i] = lv_btn_create(days_container, NULL);
        lv_obj_set_size(day_buttons[i], 42, 42);
        
        if (i == 0) {
            lv_obj_align(day_buttons[i], NULL, LV_ALIGN_IN_LEFT_MID, 0, 0);
        } else {
            lv_obj_align(day_buttons[i], day_buttons[i-1], LV_ALIGN_OUT_RIGHT_MID, 6, 0);
        }
        
        lv_obj_set_event_cb(day_buttons[i], day_btn_event_cb);
        lv_obj_t *label_day = lv_label_create(day_buttons[i], NULL);
        lv_label_set_text(label_day, day_labels[i]);
        
        day_selected[i] = false;
        lv_obj_set_style_local_bg_color(day_buttons[i], LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    }
    
    frequency_container = lv_obj_create(scrl, NULL);
    lv_obj_set_size(frequency_container, 400, 350);
    lv_obj_set_style_local_bg_opa(frequency_container, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_width(frequency_container, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_hidden(frequency_container, true);
    lv_obj_set_style_local_pad_inner(frequency_container, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    
    // calendar popup
    calendar_popup = lv_obj_create(frequency_container, NULL);
    lv_obj_set_size(calendar_popup, 380, 240);
    lv_obj_set_style_local_pad_inner(calendar_popup, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    // place calendar popup at the top of the frequency container
    lv_obj_align(calendar_popup, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
    
    lv_obj_t *cal_title = lv_label_create(calendar_popup, NULL);
    lv_label_set_text(cal_title, "Select Custom Dates");
    lv_obj_align(cal_title, NULL, LV_ALIGN_IN_TOP_MID, 0, 3);
    
    calendar = lv_calendar_create(calendar_popup, NULL);
    lv_obj_set_size(calendar, 320, 180);
    lv_obj_align(calendar, cal_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 3);
    
    // set to current date
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    lv_calendar_date_t today_date = {
        .year = tm_info->tm_year + 1900,
        .month = tm_info->tm_mon + 1,
        .day = tm_info->tm_mday
    };
    lv_calendar_set_today_date(calendar, &today_date);
    lv_calendar_set_showed_date(calendar, &today_date);
    
    // highlight selected dates
    lv_obj_set_style_local_bg_color(calendar, LV_CALENDAR_PART_DATE, LV_STATE_PRESSED, LV_COLOR_ORANGE);
    lv_obj_set_style_local_bg_color(calendar, LV_CALENDAR_PART_DATE, LV_STATE_FOCUSED, LV_COLOR_ORANGE);
    lv_obj_set_style_local_bg_color(calendar, LV_CALENDAR_PART_DATE, LV_STATE_CHECKED, LV_COLOR_ORANGE);
    lv_obj_set_style_local_text_color(calendar, LV_CALENDAR_PART_DATE, LV_STATE_CHECKED, LV_COLOR_WHITE);
    
    lv_obj_set_event_cb(calendar, calendar_event_cb);
    
    lv_obj_t *btn_cal_close = lv_btn_create(calendar_popup, NULL);
    lv_obj_set_size(btn_cal_close, 80, 30);
    lv_obj_align(btn_cal_close, calendar, LV_ALIGN_OUT_BOTTOM_MID, 0, 3);
    lv_obj_set_event_cb(btn_cal_close, calendar_close_event_cb);
    lv_obj_t *label_close = lv_label_create(btn_cal_close, NULL);
    lv_label_set_text(label_close, "Done");
    
    // selected dates label directly below the calendar area
    label_selected_dates = lv_label_create(frequency_container, NULL);
    lv_label_set_text(label_selected_dates, "None");
    lv_label_set_long_mode(label_selected_dates, LV_LABEL_LONG_BREAK);
    lv_label_set_align(label_selected_dates, LV_LABEL_ALIGN_LEFT);
    lv_obj_set_width(label_selected_dates, 380);
    lv_obj_align(label_selected_dates, calendar_popup, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);
    
    lv_obj_t *label_time = lv_label_create(scrl, NULL);
    lv_label_set_text(label_time, "Time:");
    
    dd_time_option = lv_dropdown_create(scrl, NULL);
    lv_dropdown_set_options(dd_time_option, "No time\nSpecific time\nMorning\nAfternoon\nNight");
    lv_obj_set_size(dd_time_option, 180, 35);
    lv_obj_set_event_cb(dd_time_option, time_option_event_cb);

    // label for preset times (morning/afternoon/night), placed directly under the dropdown
    label_preset_time = lv_label_create(scrl, NULL);
    lv_label_set_text(label_preset_time, "");
    lv_obj_set_width(label_preset_time, 380);
    lv_obj_set_style_local_text_color(label_preset_time, LV_LABEL_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_label_set_align(label_preset_time, LV_LABEL_ALIGN_LEFT);
    lv_obj_set_hidden(label_preset_time, true);
    
    // rollers container (for "Specific time")
    time_container = lv_obj_create(scrl, NULL);
    lv_obj_set_size(time_container, 250, 120);
    lv_obj_set_style_local_bg_opa(time_container, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_width(time_container, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);
    lv_obj_set_hidden(time_container, true);
    
    // rollers
    roller_hour = lv_roller_create(time_container, NULL);
    lv_roller_set_options(roller_hour, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller_hour, 3);
    lv_obj_set_size(roller_hour, 60, 80);
    lv_obj_align(roller_hour, NULL, LV_ALIGN_IN_LEFT_MID, 30, 10);
    lv_obj_set_hidden(roller_hour, true);
    
    roller_minute = lv_roller_create(time_container, NULL);
    lv_roller_set_options(roller_minute, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(roller_minute, 3);
    lv_obj_set_size(roller_minute, 60, 80);
    lv_obj_align(roller_minute, roller_hour, LV_ALIGN_OUT_RIGHT_MID, 30, 0);
    lv_obj_set_hidden(roller_minute, true);
    
    label_hour = lv_label_create(time_container, NULL);
    lv_label_set_text(label_hour, "Hour");
    lv_obj_align(label_hour, roller_hour, LV_ALIGN_OUT_TOP_MID, 0, -5);
    lv_obj_set_hidden(label_hour, true);
    
    label_minute = lv_label_create(time_container, NULL);
    lv_label_set_text(label_minute, "Minute");
    lv_obj_align(label_minute, roller_minute, LV_ALIGN_OUT_TOP_MID, 0, -5);
    lv_obj_set_hidden(label_minute, true);

    // buttons
    lv_obj_t *btn_container = lv_obj_create(scrl, NULL);
    lv_obj_set_size(btn_container, 350, 50);
    lv_obj_set_style_local_bg_opa(btn_container, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_TRANSP);
    lv_obj_set_style_local_border_width(btn_container, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, 0);

    btn_give_up = lv_btn_create(btn_container, NULL);
    lv_obj_set_size(btn_give_up, 90, 40);
    lv_obj_align(btn_give_up, NULL, LV_ALIGN_IN_LEFT_MID, 0, 0);
    lv_obj_set_event_cb(btn_give_up, give_up_btn_event_cb);
    lv_obj_set_style_local_bg_color(btn_give_up, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(255, 100, 100));
    lv_label_create(btn_give_up, NULL);
    lv_label_set_text(lv_obj_get_child(btn_give_up, NULL), "Give up");

    lv_obj_t *btn_cancel = lv_btn_create(btn_container, NULL);
    lv_obj_set_size(btn_cancel, 95, 40);
    lv_obj_align(btn_cancel, btn_give_up, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_set_event_cb(btn_cancel, cancel_edit_event_cb);
    lv_label_create(btn_cancel, NULL);
    lv_label_set_text(lv_obj_get_child(btn_cancel, NULL), "Cancel");
    
    btn_save = lv_btn_create(btn_container, NULL);
    lv_obj_set_size(btn_save, 95, 40);
    lv_obj_align(btn_save, btn_cancel, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_set_event_cb(btn_save, save_btn_event_cb);
    lv_obj_set_style_local_bg_color(btn_save, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(100, 255, 100));
    lv_label_create(btn_save, NULL);
    lv_label_set_text(lv_obj_get_child(btn_save, NULL), "Save");
}

void ui_manage_habits_init(void) {
    lv_obj_t *obj_page = ui_page_get_obj();
    
    list_container = lv_obj_create(obj_page, NULL);
    lv_obj_set_size(list_container, 520, 300);
    lv_obj_align(list_container, NULL, LV_ALIGN_IN_RIGHT_MID, -40, 30);
    lv_obj_set_hidden(list_container, true);
    
    lv_obj_t *list_title = lv_label_create(list_container, NULL);
    lv_label_set_text(list_title, "Your Habits");
    lv_obj_align(list_title, NULL, LV_ALIGN_IN_TOP_MID, 0, 10);

    habit_list = lv_list_create(list_container, NULL);
    lv_obj_set_size(habit_list, 480, 200);
    lv_obj_align(habit_list, list_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    
    btn_add_new = lv_btn_create(list_container, NULL);
    lv_obj_set_size(btn_add_new, 460, 45);
    lv_obj_align(btn_add_new, habit_list, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    lv_obj_set_event_cb(btn_add_new, add_new_event_cb);
    lv_obj_t *label_add = lv_label_create(btn_add_new, NULL);
    lv_label_set_text(label_add, LV_SYMBOL_PLUS " Add New Habit");
    lv_obj_set_style_local_bg_color(btn_add_new, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(100, 200, 255));
    
    ui_manage_habits_create_edit_panel();
    ui_manage_habits_add_predefined();
}

void ui_manage_habits_show(void) {
    if (list_container) {
        lv_obj_set_hidden(list_container, false);
        refresh_habit_list();
    }
}

void ui_manage_habits_hide(void) {
    if (_keyboard) {
        ui_keyboard_delete(_keyboard);
        _keyboard = NULL;
    }
    if (list_container) {
        lv_obj_set_hidden(list_container, true);
    }
    hide_edit_panel();
}

static bool ensure_habits_capacity(void) {
    if (habits == NULL) {
        habits = (habit_t *)malloc(INITIAL_HABITS_CAPACITY * sizeof(habit_t));
        if (habits == NULL) return false;
        habits_capacity = INITIAL_HABITS_CAPACITY;
        return true;
    }
    if (habit_count >= habits_capacity) {
        uint16_t new_capacity = habits_capacity * 2;
        habit_t *new_habits = (habit_t *)realloc(habits, new_capacity * sizeof(habit_t));
        if (new_habits == NULL) return false;
        habits = new_habits;
        habits_capacity = new_capacity;
    }
    return true;
}

void ui_manage_habits_add_predefined(void) {
    // need to get data from cloud/database - load user's saved habits
    habit_t predefined[] = {
        {"Drink Water", "Stay hydrated throughout the day", 0, {1,1,1,1,1,1,1}, 7, 2, 9, 0, 1, 1, {{0}}, 0, 0, 0, 0, 0, 0},
        {"Exercise", "Get moving and stay fit", 0, {1,0,1,0,1,0,0}, 3, 2, 7, 0, 1, 1, {{0}}, 0, 0, 0, 0, 0, 0},
        {"Read Book", "Read for personal growth", 0, {1,1,1,1,1,0,0}, 5, 4, 20, 0, 1, 1, {{0}}, 0, 0, 0, 0, 0, 0},
        {"Meditate", "Practice mindfulness daily", 0, {1,1,1,1,1,1,1}, 7, 2, 6, 0, 1, 1, {{0}}, 0, 0, 0, 0, 0, 0},
    };
    
    int count = sizeof(predefined) / sizeof(predefined[0]);
    for (int i = 0; i < count; i++) {
        if (ensure_habits_capacity()) {
            memcpy(&habits[habit_count], &predefined[i], sizeof(habit_t));
            habits[habit_count].streak_count = 0;
            habits[habit_count].last_completed_date = 0;
            habits[habit_count].skip_days_used = 0;
            habits[habit_count].skip_days_reset_date = 0;
            habits[habit_count].is_flexible = 1; 
            habit_count++;
        }
    }
}

static void refresh_habit_list(void) {
    lv_list_clean(habit_list);
    for (uint16_t i = 0; i < habit_count; i++) {
        if (habits[i].is_active) {
            lv_obj_t *btn = lv_list_add_btn(habit_list, LV_SYMBOL_EDIT, habits[i].name);
            lv_obj_set_event_cb(btn, habit_list_event_cb);
            lv_obj_set_user_data(btn, (void *)(uintptr_t)i);
        }
    }
}

static void show_edit_panel(int16_t habit_idx) {
    selected_habit_idx = habit_idx;
    if (habit_idx >= 0) {
        habit_t *h = &habits[habit_idx];
        lv_textarea_set_text(ta_habit_name, h->name);
        lv_textarea_set_text(ta_habit_description, h->description);
        lv_dropdown_set_selected(dd_frequency, h->frequency_type);
        
        if (h->frequency_type == 1) { 
            selected_custom_dates_count = 0;
        } else {
            selected_custom_dates_count = 0;
        }
        
        for (int i = 0; i < 7; i++) {
            day_selected[i] = h->days_selected[i];
            if (day_selected[i]) {
                lv_obj_set_style_local_bg_color(day_buttons[i], LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_ORANGE);
            } else {
                lv_obj_set_style_local_bg_color(day_buttons[i], LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
            }
        }
        
        lv_dropdown_set_selected(dd_time_option, h->time_option);
        if (h->time_option == 0) {
            lv_obj_set_hidden(time_container, true);
            lv_obj_set_hidden(label_preset_time, true);
        } else if (h->time_option == 1) {
            lv_obj_set_hidden(time_container, false);
            lv_obj_set_size(time_container, 250, 120);
            lv_obj_set_hidden(label_preset_time, true);
            lv_obj_set_hidden(label_hour, false);
            lv_obj_set_hidden(label_minute, false);
            lv_obj_set_hidden(roller_hour, false);
            lv_obj_set_hidden(roller_minute, false);
        } else {
            lv_obj_set_hidden(time_container, true);
            lv_obj_set_hidden(label_hour, true);
            lv_obj_set_hidden(label_minute, true);
            lv_obj_set_hidden(roller_hour, true);
            lv_obj_set_hidden(roller_minute, true);
            update_preset_time_label(h->time_option);
        }
        lv_roller_set_selected(roller_hour, h->hour, LV_ANIM_OFF);
        lv_roller_set_selected(roller_minute, h->minute, LV_ANIM_OFF);
        lv_obj_set_hidden(btn_give_up, false);
    } else {
        lv_textarea_set_text(ta_habit_name, "");
        lv_textarea_set_text(ta_habit_description, "");
        lv_dropdown_set_selected(dd_frequency, 0);
        selected_custom_dates_count = 0;
        
        for (int i = 0; i < 7; i++) {
            day_selected[i] = false;
            lv_obj_set_style_local_bg_color(day_buttons[i], LV_BTN_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GRAY);
        }
        lv_dropdown_set_selected(dd_time_option, 0);
        lv_obj_set_hidden(time_container, true);
        lv_obj_set_hidden(label_preset_time, true);
        lv_obj_set_hidden(btn_give_up, true);
    }
    lv_obj_set_hidden(edit_panel, false);
    lv_obj_move_foreground(edit_panel);
}

static void hide_edit_panel(void) {
    if (_keyboard) {
        ui_keyboard_delete(_keyboard);
        _keyboard = NULL;
    }
    if (calendar_popup) {
        lv_obj_set_hidden(calendar_popup, true);
    }
    lv_obj_set_hidden(edit_panel, true);
    selected_habit_idx = -1;
}

static void save_current_habit(void) {
    habit_t *h;
    bool is_new_habit = false;
    if (selected_habit_idx >= 0) {
        h = &habits[selected_habit_idx];
    } else {
        if (!ensure_habits_capacity()) return;
        h = &habits[habit_count++];
        is_new_habit = true;
    }
    
    const char *text = lv_textarea_get_text(ta_habit_name);
    strncpy(h->name, text, sizeof(h->name) - 1);
    h->name[sizeof(h->name) - 1] = '\0';
    
    const char *desc_text = lv_textarea_get_text(ta_habit_description);
    strncpy(h->description, desc_text, sizeof(h->description) - 1);
    h->description[sizeof(h->description) - 1] = '\0';
    
    h->frequency_type = lv_dropdown_get_selected(dd_frequency);
    if (h->frequency_type == 1) {
        h->num_days = selected_custom_dates_count;
        for (int i = 0; i < selected_custom_dates_count && i < 31; i++) {
            h->custom_dates[i].year = selected_custom_dates[i].year;
            h->custom_dates[i].month = selected_custom_dates[i].month;
            h->custom_dates[i].day = selected_custom_dates[i].day;
        }
    } else {
        h->num_days = 0;
        for (int i = 0; i < 7; i++) {
            h->days_selected[i] = day_selected[i];
            if (day_selected[i]) {
                h->num_days++;
            }
        }
    }
    
    uint16_t time_opt = lv_dropdown_get_selected(dd_time_option);
    h->time_option = time_opt;
    if (time_opt == 1) {
        h->hour = lv_roller_get_selected(roller_hour);
        h->minute = lv_roller_get_selected(roller_minute);
    } else if (time_opt == 2) {
        h->hour = morn_hr;
        h->minute = morn_min;
    } else if (time_opt == 3) {
        h->hour = afternoon_hr;
        h->minute = afternoon_min;
    } else if (time_opt == 4) {
        h->hour = night_hr;
        h->minute = night_min;
    }
    
    h->show_in_weekly_todo = 1;
    h->is_active = 1;
    
    if (is_new_habit) {
        h->streak_count = 0;
        h->last_completed_date = 0;
        h->skip_days_used = 0;
        h->skip_days_reset_date = 0;
        h->is_flexible = 1;
    }
}

habit_t* ui_manage_habits_get_habit(uint16_t index) {
    if (index < habit_count) {
        return &habits[index];
    }
    return NULL;
}

void ui_manage_habits_mark_completed(uint16_t index, bool completed) {
    if (index < habit_count) {
        habit_t *h = &habits[index];
        time_t now = time(NULL);
        
        if (completed) {
            time_t last_completion = (time_t)h->last_completed_date;
            int days_diff = (now - last_completion) / (24 * 60 * 60);

            time_t skip_reset = (time_t)h->skip_days_reset_date;
            int days_since_reset = (now - skip_reset) / (24 * 60 * 60);
            if (days_since_reset >= 14) {
                h->skip_days_used = 0;
                h->skip_days_reset_date = (uint32_t)now;
            }
            
            int days_missed = (days_diff > 1) ? days_diff - 1 : 0;
            if (h->skip_days_used + days_missed <= 2) {
                h->skip_days_used += days_missed;
                h->streak_count++;
            } else {
                h->streak_count = 1;
                h->skip_days_used = 0;
                h->skip_days_reset_date = (uint32_t)now;
            }
            
            h->last_completed_date = (uint32_t)now;
        } else {
            if (h->streak_count > 0) {
                h->streak_count--;
            }
            h->last_completed_date = 0;
        }
    }
}

uint8_t ui_manage_habits_get_remaining_skips(const habit_t *habit) {
    if (!habit) {
        return 0;
    }
    time_t now = time(NULL);
    time_t skip_reset = (time_t)habit->skip_days_reset_date;
    int days_since_reset = (now - skip_reset) / (24 * 60 * 60);
    if (days_since_reset >= 14) {
        return 2;
    }
    return (2 - habit->skip_days_used);
}
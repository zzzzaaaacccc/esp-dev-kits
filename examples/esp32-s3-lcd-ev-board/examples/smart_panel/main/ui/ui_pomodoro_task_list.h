#ifndef UI_POMODORO_TASK_LIST_H
#define UI_POMODORO_TASK_LIST_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TASKS 50
#define MAX_TASK_LEN 128

typedef struct {
    char text[MAX_TASK_LEN];
    bool completed;
    uint32_t created_time;
} task_t;

void ui_task_list_init(lv_obj_t *parent_container);
void ui_task_list_show(void);
void ui_task_list_hide(void);
void ui_task_list_add_task(const char *task_text);
void ui_task_list_remove_task(uint16_t index);
uint16_t ui_task_list_get_count(void);
lv_obj_t *ui_task_list_get_input_obj(void);

#ifdef __cplusplus
}
#endif

#endif  // UI_POMODORO_TASK_LIST_H

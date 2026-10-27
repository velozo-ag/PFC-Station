#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdbool.h>
#include "u8g2.h"

typedef char string[50];

typedef enum {
    UI_STATE_MAIN_MENU = 0,
    UI_STATE_SCANNER,
    UI_STATE_CONTROLLER,
    UI_STATE_NODE_VIEW,
    UI_STATE_LAB_TOOLS
} ui_state_t;

typedef struct {
    int id;
    string *name;
} system_status_t;

extern string system_status_l[];

extern const uint8_t *menu_options_img_l[];
extern const char *menu_options_l[];

extern const char *scanner_options_l[];
extern const char *saved_nodes_l[];
extern const char *controller_options_l[];
extern const char *lab_tools_options_l[];

void display_init(void);
void task_display_draw(void *pvParameters);
int display_status_update(int new_system_status);
void display_show_menu(void);
void display_show_scanner(void);
void display_update(void);

#endif

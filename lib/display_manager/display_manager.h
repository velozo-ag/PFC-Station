#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "u8g2.h"

typedef char string[50];

typedef struct{
    int id;
    string *name;
} system_status_t;


// extern const system_status_t system_status_l[];
extern string system_status_l[];

extern const uint8_t *menu_options_img_l[];
extern const char *menu_options_l[];

extern const char *scanner_options_l[];
extern const char *saved_nodes_l[];
extern const char *controller_options_l[];
extern const char *transmisor_options_l[];

void display_init(void);
void task_display_draw(void *pvParameters);
int display_status_update(int new_system_status);
void display_show_menu(void);

void display_show_scanner(void);

void display_update(void);

#endif
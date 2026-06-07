#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "u8g2.h"

typedef char string[12];

extern const uint8_t *system_status_img_l[];
extern string system_status_l[];

void display_init(void);
void task_display_draw(void *pvParameters);
void display_status_update(int new_system_status);
void display_show_menu(void);
void display_update(void);

#endif
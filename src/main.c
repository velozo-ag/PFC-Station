#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "u8g2.h"             
#include "display_manager.h"


void app_main(void)
{
  xTaskCreatePinnedToCore(
    task_display_draw,
    "task_display",
    2000,
    NULL,
    1,
    NULL,
    1
  );

}

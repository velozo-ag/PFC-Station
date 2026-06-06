#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "u8g2.h"             
#include "display_manager.h"


void app_main(void)
{
  display_init();

  display_show_menu();

  display_update();

  while(1){
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
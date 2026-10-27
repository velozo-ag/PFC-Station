#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "u8g2.h"
#include "display_manager.h"
#include "input_manager.h"
#include "system_types.h"
#include "radio_manager.h"

QueueHandle_t button_queue;

void app_main(void)
{
  button_queue = xQueueCreate(10, sizeof(system_message));
  if(button_queue == NULL){
    printf("[Error] No se pudo crear la queue.\n");
    return;
  }

  radio_manager_init();

  xTaskCreatePinnedToCore(
      task_radio_manager,
      "task_radio",
      4096,
      NULL,
      2,
      NULL,
      0);

  xTaskCreatePinnedToCore(
      task_display_draw,
      "task_display",
      2048,
      NULL,
      1,
      NULL,
      1);

  vTaskDelay(500 / portTICK_PERIOD_MS);
  input_button_init();
}

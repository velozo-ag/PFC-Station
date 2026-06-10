#include "input_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <freertos/queue.h>
#include "driver/gpio.h"
#include <esp_timer.h>
#include "config.h"
#include "system_types.h"

#define DEBOUNCE_DELAY 200000ULL

static volatile uint64_t last_isr_time[4] = {0, 0, 0, 0};
// static volatile uint64_t last_isr_time = 0;
static volatile uint32_t counter = 0;

extern QueueHandle_t button_queue;

static void IRAM_ATTR button_isr_handler(void *arg)
{
    uint32_t button_id = (uint32_t)arg;

    uint64_t now = esp_timer_get_time();

    // if (now - last_isr_time > DEBOUNCE_DELAY)
    if (now - last_isr_time[button_id] > DEBOUNCE_DELAY)
    {

        last_isr_time[button_id] = now;
        // last_isr_time = now;
        system_message msg;

        switch (button_id)
        {
        case 0:
            msg.text = "UP";
            msg.id = 0;
            break;
            case 1:
            msg.text = "DOWN";
            msg.id = 1;
            break;
            case 2:
            msg.text = "ENTER";
            msg.id = 2;
            break;
            default:
            msg.text = "UNKNOWN";
            msg.id = -1;
            break;
        }

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(button_queue, &msg, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken)
        {
            portYIELD_FROM_ISR();
        }
    }
}

void input_button_init(void)
{

    uint64_t pin_bit_mask = (1ULL << PIN_BTN_UP) |
                            (1ULL << PIN_BTN_DOWN) |
                            (1ULL << PIN_BTN_ENTER);
    // | (1ULL << PIN_BTN_GPIO),

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        // .pin_bit_mask = (1ULL << PIN_BTN_GPIO),
        .pin_bit_mask = pin_bit_mask, // aca pines mappeados
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE};

    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);

    // a cada pin se le asigna el un mismo handler. el ultimo parametro es el *args que recibe button_isr_handler, un id en este caso
    gpio_isr_handler_add(PIN_BTN_UP, button_isr_handler, (void *)0);
    gpio_isr_handler_add(PIN_BTN_DOWN, button_isr_handler, (void *)1);
    gpio_isr_handler_add(PIN_BTN_ENTER, button_isr_handler, (void *)2);
};

void task_input_button(void *pvParameters)
{
    input_button_init();
    system_message recived_msg;

    while (1)
    {
        if (xQueueReceive(button_queue, &recived_msg, portMAX_DELAY))
        {
            printf("[Input] button %s pressed\n", recived_msg.text);
        }
    }
};

void input_button_handle(void) {

};

void input_button_update(void) {

};

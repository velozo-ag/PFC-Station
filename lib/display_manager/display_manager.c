#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "config.h"
#include "system_types.h"
#include "display_assets.h"
#include "display_manager.h"
#include "u8g2_esp32_hal.h"

static u8g2_t u8g2;

string system_status_l[] = {"Menu", "Scanner", "Controller", "Lab Tools"};

const uint8_t* menu_options_img_l[] = {image_radar_bits, image_menuroller_bits,
                                       image_transmitter_bits};
const char* menu_options_l[] = {"scanner", "controller", "lab tools"};

const char* scanner_options_l[] = {
    "< Volver", "Hibrid node   H/E-NOW", "Reader node   R/LoRa",
    "Actuator node A/E-NOW", "Weather node  W/LoRa"};
const char* saved_nodes_l[] = {"< Volver", "Node 01 [Blinker/Rele]",
                               "Node 02 [Dual-Servo]",
                               "Node 03 [Sensor Meteo]"};
const char* controller_options_l[] = {
    "< Volver", "Rele 1           OFF", "Rele 2            ON",
    "Servo 1       <  90>", "Bomba de agua    OFF"};
const char* lab_tools_options_l[] = {"< Volver", "Sniffer RF (LoRa)",
                                     "Packet Injector", "GPIO Lab (Dual-Servo)",
                                     "SD Apps Launcher"};

// mock states
static bool mock_rele_state = false;
static int mock_servo_angle = 90;
static int mock_mode_idx = 0;
static const char* mock_modes[] = {"OFF", "SLOW", "FAST", "AUTO"};
static const int mock_modes_count = 4;

static const char** menu_options_actual;

static int menu_length;
static ui_state_t system_status;
static int menu_option;
static int scroll_offset;
static bool is_editing = false;

extern QueueHandle_t button_queue;

// segun la linea, sobreescribe el widget correspondiente en ese espacio de
// memoria buffer
static void get_node_widget_line(int index, char* buf, size_t max_len) {
    switch (index) {
        case 0:
            snprintf(buf, max_len, "< Volver");
            break;
        case 1:
            snprintf(buf, max_len, "Rele 1        [%s]",
                     mock_rele_state ? " ON" : "OFF");
            break;
        case 2:
            if (is_editing && menu_option == 2) {
                snprintf(buf, max_len, "Servo:      [%3d deg]*",
                         mock_servo_angle);
            } else {
                snprintf(buf, max_len, "Servo:       <%3d deg>",
                         mock_servo_angle);
            }
            break;
        case 3:
            if (is_editing && menu_option == 3) {
                snprintf(buf, max_len, "Modo:       [%s]*",
                         mock_modes[mock_mode_idx]);
            } else {
                snprintf(buf, max_len, "Modo:        <%s>",
                         mock_modes[mock_mode_idx]);
            }
            break;
        case 4:
            snprintf(buf, max_len, "Temp Sensor   24.5 C");
            break;
        case 5:
            snprintf(buf, max_len, "Diag Heap    182 KB");
            break;
        default:
            snprintf(buf, max_len, "Widget %d", index);
            break;
    }
}

/*
Inicializa el display con u8g2
hacemos uso de drivers del display
inicializamos variables de menu
*/
void display_init(void) {
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.i2c.sda = DISPLAY_SDA;
    hal.bus.i2c.scl = DISPLAY_SCL;

    u8g2_esp32_hal_init(hal);

    u8g2_Setup_ssd1306_i2c_128x64_noname_f(
        &u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);

    u8g2_SetI2CAddress(&u8g2, 0x78);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);

    system_status = UI_STATE_MAIN_MENU;
    menu_option = 0;
    menu_options_actual = menu_options_l;
    menu_length = sizeof(menu_options_l) / sizeof(menu_options_l[0]);
    scroll_offset = 0;
    is_editing = false;

    display_show_menu();
}

/*
Tarea concurrente en Core FreeRTOS
Dibuja display y gestiona maquina de estados de la UI
*/
void task_display_draw(void* pvParameters) {
    display_init();
    system_message recived_msg;

    while (1) {
        if (xQueueReceive(button_queue, &recived_msg, portMAX_DELAY)) {
            printf("[Input] button %s (status=%d, opt=%d, editing=%d)\n",
                   recived_msg.text, system_status, menu_option, is_editing);

            switch (recived_msg.id) {
                case 0:  // UP
                    if (is_editing) {
                        if (system_status == UI_STATE_NODE_VIEW) {
                            // si esta en widget, navega en widget (servo o
                            // lista)
                            if (menu_option == 2) {
                                if (mock_servo_angle + 5 <= 180)
                                    mock_servo_angle += 5;
                            } else if (menu_option == 3) {
                                mock_mode_idx =
                                    (mock_mode_idx + 1) % mock_modes_count;
                            }
                        }

                        // si no esta editando, navega en menu
                    } else {
                        menu_option--;
                        if (menu_option < 0) {
                            menu_option = menu_length - 1;
                        }
                    }
                    break;

                case 1:  // DOWN
                    if (is_editing) {
                        if (system_status == UI_STATE_NODE_VIEW) {
                            // si esta en widget, navega en widget (servo o
                            // lista)
                            if (menu_option == 2) {
                                if (mock_servo_angle - 5 >= 0)
                                    mock_servo_angle -= 5;
                            } else if (menu_option == 3) {
                                mock_mode_idx =
                                    (mock_mode_idx - 1 + mock_modes_count) %
                                    mock_modes_count;
                            }
                        }
                    } else {
                        menu_option++;
                        if (menu_option >= menu_length) {
                            menu_option = 0;
                        }
                    }
                    break;

                case 2:  // ENTER
                    if (system_status == UI_STATE_MAIN_MENU) {
                        // Desde el main_menu ingresa a:
                        // 0_Scanner, 1_Controller, 2_Lab Tools
                        if (menu_option == 0)
                            display_status_update(UI_STATE_SCANNER);
                        else if (menu_option == 1)
                            display_status_update(UI_STATE_CONTROLLER);
                        else if (menu_option == 2)
                            display_status_update(UI_STATE_LAB_TOOLS);

                    } else {
                        // si esta editando, se hace submit del widget
                        if (is_editing) {
                            is_editing = false;
                            printf("[UI] Finished editing widget %d\n",
                                   menu_option);
                        } else {
                            // "< Volver"
                            if (menu_option == 0) {
                                if (system_status == UI_STATE_NODE_VIEW) {
                                    display_status_update(UI_STATE_CONTROLLER);
                                } else {
                                    display_status_update(UI_STATE_MAIN_MENU);
                                }

                            } else {
                                // si se encuentra en Controller, entra dentro
                                // del nodo
                                if (system_status == UI_STATE_CONTROLLER) {
                                    display_status_update(UI_STATE_NODE_VIEW);

                                    // si esta dentro del nodo, acciona widgets
                                } else if (system_status ==
                                           UI_STATE_NODE_VIEW) {
                                    // Switch toggle
                                    if (menu_option == 1) {
                                        mock_rele_state = !mock_rele_state;
                                        printf("[UI] Toggled Rele: %s\n",
                                               mock_rele_state ? "ON" : "OFF");

                                        // Slider o Selector
                                    } else if (menu_option == 2 ||
                                               menu_option == 3) {
                                        is_editing = true;
                                        printf(
                                            "[UI] Edit mode active on widget "
                                            "%d\n",
                                            menu_option);
                                    }

                                } else if (system_status == UI_STATE_SCANNER) {
                                    printf("[UI] Scanner item %d pressed\n",
                                           menu_option);
                                } else if (system_status ==
                                           UI_STATE_LAB_TOOLS) {
                                    printf("[UI] Lab Tools item %d pressed\n",
                                           menu_option);
                                }
                            }
                        }
                    }
                    break;

                default:
                    break;
            }

            if (system_status == UI_STATE_MAIN_MENU) {
                display_show_menu();
            } else {
                display_show_scanner();
            }
        }
    }
}

/*
Handle de status_update. Settea largo del menu
y contenido segun opcion seleccionada
*/
int display_status_update(int option) {
    system_status = (ui_state_t)option;
    is_editing = false;

    switch (system_status) {
        case UI_STATE_SCANNER:
            menu_length =
                sizeof(scanner_options_l) / sizeof(scanner_options_l[0]);
            menu_options_actual = scanner_options_l;
            break;
        case UI_STATE_CONTROLLER:
            menu_length = sizeof(saved_nodes_l) / sizeof(saved_nodes_l[0]);
            menu_options_actual = saved_nodes_l;
            break;
        case UI_STATE_NODE_VIEW:
            menu_length = 6;
            menu_options_actual = NULL;
            break;
        case UI_STATE_LAB_TOOLS:
            menu_length =
                sizeof(lab_tools_options_l) / sizeof(lab_tools_options_l[0]);
            menu_options_actual = lab_tools_options_l;
            break;
        default:
            system_status = UI_STATE_MAIN_MENU;
            menu_length = sizeof(menu_options_l) / sizeof(menu_options_l[0]);
            menu_options_actual = menu_options_l;
            break;
    }

    menu_option = 0;
    scroll_offset = 0;
    return (int)system_status;
}

/*
Show del main_menu / home de la estacion
*/
void display_show_menu(void) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetBitmapMode(&u8g2, 1);
    u8g2_SetFontMode(&u8g2, 1);

    // battery
    u8g2_SetFont(&u8g2, u8g2_font_4x6_tr);
    u8g2_DrawStr(&u8g2, 5, 10, "75%");

    char title[30] = "| Select: ";
    strlcat(title, menu_options_l[menu_option], sizeof(title));

    // head data - station address - time
    u8g2_DrawStr(&u8g2, 19, 10, title);
    u8g2_DrawStr(&u8g2, 2, 38, "MAC: 00:1A:2B:3C:4D:5E");
    u8g2_SetFont(&u8g2, u8g2_font_timR14_tr);
    u8g2_DrawStr(&u8g2, 2, 30, "11:21 a.m.");

    // btm text
    u8g2_SetFont(&u8g2, u8g2_font_4x6_tr);
    u8g2_DrawStr(&u8g2, 5, 59, "3 Nodes sync | 2 - 1");

    int top_i = (menu_option + 2) % 3;
    int btm_i = (menu_option + 1) % 3;
    u8g2_DrawXBM(&u8g2, 105, -5, 21, 21, menu_options_img_l[top_i]);
    u8g2_DrawXBM(&u8g2, 105, 49, 21, 21, menu_options_img_l[btm_i]);
    u8g2_DrawXBM(&u8g2, 105, 22, 21, 21, menu_options_img_l[menu_option]);

    // layout
    u8g2_DrawXBM(&u8g2, 2, 2, 126, 61, image_layout_bits);
    u8g2_SendBuffer(&u8g2);
}

/*
Show menu modular segun sea
main_menu, scanner, controller, node_list / widgets
se encarga de renderizar toda subpantalla en formato lista
*/
void display_show_scanner(void) {
    u8g2_ClearBuffer(&u8g2);
    u8g2_SetBitmapMode(&u8g2, 1);
    u8g2_SetFontMode(&u8g2, 1);

    u8g2_DrawXBM(&u8g2, 2, 2, 124, 61, image_paint_7_bits);
    u8g2_SetFont(&u8g2, u8g2_font_4x6_tr);
    u8g2_DrawStr(&u8g2, 5, 10, "75%");

    switch (system_status) {
        case UI_STATE_SCANNER:
            u8g2_DrawStr(&u8g2, 19, 10, "| Scanner Mode");
            break;
        case UI_STATE_CONTROLLER:
            u8g2_DrawStr(&u8g2, 19, 10, "| Controller Mode");
            break;
        case UI_STATE_NODE_VIEW:
            u8g2_DrawStr(&u8g2, 19, 10, "| Node 01 Widgets");
            break;
        case UI_STATE_LAB_TOOLS:
            u8g2_DrawStr(&u8g2, 19, 10, "| Lab Tools Mode");
            break;
        default:
            break;
    }

    if (is_editing) {
        u8g2_DrawStr(&u8g2, 90, 10, "[EDIT]");
    }

    int max_menu_items = 5;

    if (menu_option < scroll_offset)  // empuja el tope de arriba
    {
        scroll_offset = menu_option;
    }

    bool hidden_items = (scroll_offset + max_menu_items < menu_length);
    int view = hidden_items ? (max_menu_items - 1) : max_menu_items;

    if (menu_option >= scroll_offset + view)  // empuja el tope para abajo
    {
        scroll_offset = menu_option - view + 1;
    }

    for (int i = 0; i < view; i++) {
        int index = i + scroll_offset;
        if (index >= menu_length) {
            break;
        }

        int y_pos = 21 + (i * 7);
        char buffer_linea[40];
        char item_text[35];

        if (system_status == UI_STATE_NODE_VIEW) {
            get_node_widget_line(index, item_text, sizeof(item_text));
        } else if (menu_options_actual != NULL) {
            strlcpy(item_text, menu_options_actual[index], sizeof(item_text));
        } else {
            item_text[0] = '\0';
        }

        if (index == menu_option) {
            if (is_editing) {
                snprintf(buffer_linea, sizeof(buffer_linea), "* %s", item_text);
            } else {
                snprintf(buffer_linea, sizeof(buffer_linea), "> %s", item_text);
            }

            u8g2_SetDrawColor(&u8g2, 2);
            u8g2_DrawBox(&u8g2, 0, y_pos - 7, 128, 9);
        } else {
            snprintf(buffer_linea, sizeof(buffer_linea), "  %s", item_text);
        }

        u8g2_DrawStr(&u8g2, 5, y_pos, buffer_linea);
    }

    if (hidden_items) {
        int y_pos_puntos = 21 + (4 * 7);
        u8g2_DrawStr(&u8g2, 5, y_pos_puntos, "  ...");
    }

    u8g2_SendBuffer(&u8g2);
}

// funcion generica de update del buffer u8g2
void display_update(void) { u8g2_SendBuffer(&u8g2); }

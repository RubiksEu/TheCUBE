#ifndef DEFINES_CUBE
#define DEFINES_CUBE

// #define SERVER_IP "97.164.101.254"
// #define SERVER_IP "77.164.101.254"
// #define SERVER_PORT 7778

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 7778

#define DEVICE_ID "CUBE_4.1

//--- DEFINES ------------------------------
#define DEBUG_STREAM SerialUSB
#define MODEM_STREAM Serial1

#define VCC1_ENABLE 3
#define N2_ENABLE 7
#define LED_BLUE 10
#define LED_RED 13
#define VCC2_ENABLE 43
#define RGB_LED_PIN 6
#define BUZZER 35

#define POWER_ON_PIN 12

#define LIGHT_VALUE_PIN A1

#define WAKE_UP_PIN 36
#define BATT_VOLTAGE_PIN A4
#define SOUND_VALUE_PIN A3
#define BATT_VOLTAGE_ENABLE 11
#define RELAY_IN1 31
#define RELAY_IN2 2
#define BATT_CHARGING 30
#define FAN 5
#define PIR_PIN 29
#define PIR_ANALOG_PIN A5
#define ENC_BUTTON 26
#define ENC_A 24
#define ENC_B 15
#define USE_R4_MODEM 0
#define OLED_RESET 4

#define MENU_START_POS_X 36
#define MENU_START_POS_Y 6
#define NEW_DISPLAY_LINE_1 12
#define MENU_START_POS_X_VALUE (MENU_START_POS_X + 35)

#define NR_OF_MENU_OPTIONS 4
#define ACTUATORS_TO_BE_DISPLAYED 4

#define MAIN_MENU 0
#define ACC_MENU 1
#define NET_MENU 2
#define ACT_MENU 3

#define LED_FLASH_MS 100

#define SOFT_RESET_COUNTER 10

#define NVM_MEMORY ((volatile uint16_t *)0x000000)
#define APP_START 0x00002004

#define DEBUG_STREAM SerialUSB
#define MODEM_STREAM Serial1

#define VCC1_ENABLE 3
#define N2_ENABLE 7
#define LED_BLUE 10
#define LED_RED 13
#define VCC2_ENABLE 43
#define RGB_LED_PIN 6
#define BUZZER 35

#define POWER_ON_PIN 12

#define LIGHT_VALUE_PIN A1

#define WAKE_UP_PIN 36
#define BATT_VOLTAGE_PIN A4
#define SOUND_VALUE_PIN A3
#define BATT_VOLTAGE_ENABLE 11
#define RELAY_IN1 31
#define RELAY_IN2 2
#define BATT_CHARGING 30
#define FAN 5
#define PIR_PIN 29
#define PIR_ANALOG_PIN A5
#define ENC_BUTTON 26
#define ENC_A 24
#define ENC_B 15

#define SEND_DATA_INTERVAL 8000
#define MAIN_LOOP_INTERVAL 200
//-----------------------------------------------------------------------------

struct menuItem {
  char* name;
  bool selected;
};

#endif

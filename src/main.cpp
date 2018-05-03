#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <stdlib.h>
#include <string.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>

#include "Sodaq_HTS221.h"
#include "Sodaq_LPS22HB.h"
#include "font.h"
#include "lwm2mclient.h"
#include "connection.h"

#ifdef DEBUG_LOG
#include "ArduinoLog.h"
#endif

//--- DEFINES ------------------------------
#ifdef DEBUG_LOG
#define DEBUG_STREAM SerialUSB
#endif

#define MODEM_STREAM Serial1
#define VCC1_ENABLE 3
#define N2_ENABLE 7
#define LED_BLUE 10
#define COLLISION 12
#define LED_RED 13
#define VCC2_ENABLE 43
#define RGB_LED_PIN 6
#define BUZZER 35
#define WAKE_UP_PIN 36
#define BATT_VOLTAGE_ENABLE 11
#define FLAME_VALUE_PIN A0
#define LIGHT_VALUE_PIN A1
#define ROTARY_VALUE_PIN A2
#define SOUND_VALUE A3
#define BATT_VOLTAGE_PIN A5
#define AIR_QUALITY A6
#define BATT_CHARGING_VALUE A7
#define BACKUP_OBJECT_COUNT 2
#define MAX_PACKET_SIZE 1024

#define RELAY_IN1 31
#define RELAY_IN2 2

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

typedef enum {
    NO_EVENT = 0,
    EVENT_BUTTON,
    EVENT_COLLISION,
    EVENT_5_MIN
} cube3_event_state_t;
cube3_event_state_t cube_state = NO_EVENT;

//--- VARIABLES ----------------------------
Sodaq_nbIOT nbiot;
Sodaq_LPS22HB lps22hb;
unsigned char update_header_counter;

uint8_t ber;
uint8_t menu_cursor = 0;
uint8_t commands_count = 0;
uint8_t reconnection_times = 0;
int8_t rssi = 1;
uint8_t loading_array[] = {6, 10, 18, 20, 16, 12, 6, 10, 6};
int16_t sensorValue;
int16_t battery_header = 300;
time_t event_sending;
unsigned char wake_up_state;
File config;
ConfigJson *commands_config = NULL;
bool isSendingData = false;
int g_reboot = 0;
static int g_quit = 0;

#ifdef DEBUG_LOG
char tmpString[40];
#endif

void displayAntenna();

void displayBattery(uint16_t battery);

void waitResetLoop(bool resetModem);

lwm2m_object_t *backupObjectArray[BACKUP_OBJECT_COUNT];

#define OBJ_COUNT 18
lwm2m_object_t *objArray[OBJ_COUNT];

typedef struct {
    lwm2m_object_t *securityObjP;
    lwm2m_object_t *serverObject;
    int sock;

    connection_t *connList;

    int addressFamily;
} client_data_t;

#ifdef LWM2M_EMBEDDED_MODE

static void prv_value_change(void *context,
                             const char *uriPath,
                             const char *value,
                             size_t valueLength)
{
    lwm2m_uri_t uri;
    if (lwm2m_stringToUri(uriPath, strlen(uriPath), &uri))
    {
        handle_value_changed(context, &uri, value, valueLength);
    }
}

void init_value_change(lwm2m_context_t *lwm2m)
{
    system_setValueChangedHandler(lwm2m, prv_value_change);
}

#else

void system_reboot() {
    NVIC_SystemReset();
}

#endif

void handle_sigint(int signum) {
    g_quit = 2;
    waitResetLoop(false);
}

void handle_value_changed(lwm2m_context_t *lwm2mH,
                          lwm2m_uri_t *uri,
                          const char *value,
                          size_t valueLength) {
    lwm2m_object_t *object = (lwm2m_object_t *) LWM2M_LIST_FIND(lwm2mH->objectList, uri->objectId);

    if (NULL != object) {
        if (object->writeFunc != NULL) {
            lwm2m_data_t *dataP;
            int result;

            dataP = lwm2m_data_new(1);
            if (dataP == NULL) {
#ifdef DEBUG_LOG
                DEBUG_STREAM.println("Internal allocation failure !\n");
#endif
                return;
            }
            dataP->id = uri->resourceId;
            lwm2m_data_encode_nstring(value, valueLength, dataP);

            result = object->writeFunc(uri->instanceId, 1, dataP, object);
            if (COAP_405_METHOD_NOT_ALLOWED == result) {
                switch (uri->objectId) {
                    case LWM2M_DEVICE_OBJECT_ID:
                        result = device_change(dataP, object);
                        break;
                    default:
                        break;
                }
            }

            if (COAP_204_CHANGED != result) {
#ifdef DEBUG_LOG
                DEBUG_STREAM.println("Failed to change value!\n");
#endif
            } else {
#ifdef DEBUG_LOG
                DEBUG_STREAM.println("value changed!\n");
#endif
                lwm2m_resource_value_changed(lwm2mH, uri);
            }
            lwm2m_data_free(1, dataP);
            return;
        } else {
#ifdef DEBUG_LOG
            DEBUG_STREAM.println("write not supported for specified resource!\n");
#endif
        }
        return;
    } else {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Object not found !\n");
#endif
    }
}

void *lwm2m_connect_server(uint16_t secObjInstID,
                           void *userData) {
    client_data_t *dataP;
    char *uri;
    char *host;
    char *port;
    connection_t *newConnP = NULL;

    dataP = (client_data_t *) userData;

    uri = get_server_uri(dataP->securityObjP, secObjInstID);

    if (uri == NULL)
        return NULL;

    // parse uri in the form "coaps://[host]:[port]"
    if (0 == strncmp(uri, "coaps://", strlen("coaps://"))) {
        host = uri + strlen("coaps://");
    } else if (0 == strncmp(uri, "coap://", strlen("coap://"))) {
        host = uri + strlen("coap://");
    } else {
        goto exit;
    }
    port = strrchr(host, ':');
    if (port == NULL)
        goto exit;
    // remove brackets
    if (host[0] == '[') {
        host++;
        if (*(port - 1) == ']') {
            *(port - 1) = 0;
        } else
            goto exit;
    }
    // split strings
    *port = 0;
    port++;

#ifdef DEBUG_LOG
    DEBUG_STREAM.print("Opening connection to server at ");
    DEBUG_STREAM.print(host);
    DEBUG_STREAM.print(":");
    DEBUG_STREAM.print(port);
#endif
    newConnP = connection_create(dataP->connList, dataP->sock, host, port, dataP->addressFamily);
    if (newConnP == NULL) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.print("Connection creation failed.\r\n");
#endif
    } else {
        dataP->connList = newConnP;
    }

    exit:
    lwm2m_free(uri);
    return (void *) newConnP;
}

void lwm2m_close_connection(void *sessionH,
                            void *userData) {
    client_data_t *app_data;
    connection_t *targetP;

    app_data = (client_data_t *) userData;
    targetP = (connection_t *) sessionH;

    if (targetP == app_data->connList) {
        app_data->connList = targetP->next;
        lwm2m_free(targetP);
    } else {
        connection_t *parentP;

        parentP = app_data->connList;
        while (parentP != NULL && parentP->next != targetP) {
            parentP = parentP->next;
        }
        if (parentP != NULL) {
            parentP->next = targetP->next;
            lwm2m_free(targetP);
        }
    }
}

static void update_event_value(lwm2m_context_t *context, const char *sensorUri, const char *stringValue) {
    lwm2m_uri_t uri;
    char value[18];
    int valueLength;
    if (lwm2m_stringToUri(sensorUri, strlen(sensorUri), &uri)) {
        valueLength = sprintf(value, "%s", stringValue);
#ifdef DEBUG_LOG
        DEBUG_STREAM.println(stringValue);
#endif
        handle_value_changed(context, &uri, value, valueLength);
    }
}

static void update_sensor_value(lwm2m_context_t *context, const char *sensorUri, double sensorValue) {
    lwm2m_uri_t uri;
    char value[18];
    int valueLength;
    if (lwm2m_stringToUri(sensorUri, strlen(sensorUri), &uri)) {
        valueLength = sprintf(value, "%f", sensorValue);
#ifdef DEBUG_LOG
        DEBUG_STREAM.println(sensorValue);
#endif
        handle_value_changed(context, &uri, value, valueLength);
    }
}

static void prv_add(char *buffer,
                    void *user_data) {
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *) user_data;
    lwm2m_object_t *objectP;
    lwm2m_object_t *objectPTemp;
    int res;

    // objectP = get_test_object();
    if (objectP == NULL) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Creating object 31024 failed.\r\n");
#endif
        return;
    }
    res = lwm2m_add_object(lwm2mH, objectP);

    if (res != 0) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.print("Adding object 31024 failed: ");
        DEBUG_STREAM.println(res);
#endif
    } else {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Object 31024 added.\r\n");
#endif
    }
    return;
}

static void prv_remove(char *buffer,
                       void *user_data) {
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *) user_data;
    int res;

    res = lwm2m_remove_object(lwm2mH, 31024);
    if (res != 0) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.print("Removing object 31024 failed: ");
        DEBUG_STREAM.println(res);
#endif
    } else {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Object 31024 removed.\r\n");
#endif
    }
    return;
}

#ifdef LWM2M_BOOTSTRAP

static void prv_initiate_bootstrap(char *buffer,
                                   void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    lwm2m_server_t *targetP;

    // HACK !!!
    lwm2mH->state = STATE_BOOTSTRAP_REQUIRED;
    targetP = lwm2mH->bootstrapServerList;
    while (targetP != NULL)
    {
        targetP->lifetime = 0;
        targetP = targetP->next;
    }
}

static void prv_backup_objects(lwm2m_context_t *context)
{
    uint16_t i;

    for (i = 0; i < BACKUP_OBJECT_COUNT; i++)
    {
        if (NULL != backupObjectArray[i])
        {
            switch (backupObjectArray[i]->objID)
            {
            case LWM2M_SECURITY_OBJECT_ID:
                clean_security_object(backupObjectArray[i]);
                lwm2m_free(backupObjectArray[i]);
                break;
            case LWM2M_SERVER_OBJECT_ID:
                clean_server_object(backupObjectArray[i]);
                lwm2m_free(backupObjectArray[i]);
                break;
            default:
                break;
            }
        }
        backupObjectArray[i] = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));
        memset(backupObjectArray[i], 0, sizeof(lwm2m_object_t));
    }

    /*
     * Backup content of objects 0 (security) and 1 (server)
     */
    copy_security_object(backupObjectArray[0],
                         (lwm2m_object_t *)LWM2M_LIST_FIND(context->objectList, LWM2M_SECURITY_OBJECT_ID));
    copy_server_object(backupObjectArray[1],
                       (lwm2m_object_t *)LWM2M_LIST_FIND(context->objectList, LWM2M_SERVER_OBJECT_ID));
}

static void prv_restore_objects(lwm2m_context_t *context)
{
    lwm2m_object_t *targetP;

    /*
     * Restore content  of objects 0 (security) and 1 (server)
     */
    targetP = (lwm2m_object_t *)LWM2M_LIST_FIND(context->objectList, LWM2M_SECURITY_OBJECT_ID);
    // first delete internal content
    clean_security_object(targetP);
    // then restore previous object
    copy_security_object(targetP, backupObjectArray[0]);

    targetP = (lwm2m_object_t *)LWM2M_LIST_FIND(context->objectList, LWM2M_SERVER_OBJECT_ID);
    // first delete internal content
    clean_server_object(targetP);
    // then restore previous object
    copy_server_object(targetP, backupObjectArray[1]);

// restart the old servers
#ifdef DEBUG_LOG
    DEBUG_STREAM.println("[BOOTSTRAP] ObjectList restored\r\n");
#endif
}

static void update_bootstrap_info(lwm2m_client_state_t *previousBootstrapState,
                                  lwm2m_context_t *context)
{
    if (*previousBootstrapState != context->state)
    {
        *previousBootstrapState = context->state;
        switch (context->state)
        {
        case STATE_BOOTSTRAPPING:
#ifdef WITH_LOGS
            DEBUG_STREAM.println("[BOOTSTRAP] backup security and server objects\r\n");
#endif
            prv_backup_objects(context);
            break;
        default:
            break;
        }
    }
}

static void close_backup_object()
{
    int i;
    for (i = 0; i < BACKUP_OBJECT_COUNT; i++)
    {
        if (NULL != backupObjectArray[i])
        {
            switch (backupObjectArray[i]->objID)
            {
            case LWM2M_SECURITY_OBJECT_ID:
                clean_security_object(backupObjectArray[i]);
                lwm2m_free(backupObjectArray[i]);
                break;
            case LWM2M_SERVER_OBJECT_ID:
                clean_server_object(backupObjectArray[i]);
                lwm2m_free(backupObjectArray[i]);
                break;
            default:
                break;
            }
        }
    }
}

#endif

//***************************************************************************
// 2017-12-01
// clears terminal
//---------------------------------------------------------------------------
void clear_OLED_terminal() {
    display.clearDisplay();
    display.display();
};
//===========================================================================

//*************************************************************************
// 2016-11-02
// Description:
//
//-------------------------------------------------------------------------
/*
 * Delay for making sound
 */
void timing_udelay(unsigned short freq) {
    unsigned short count;
    float a, b, c;

    a = 1.11;
    b = 2.21;
    c = 1.23;
    for (count = 1; count < freq; count++) {
        c = a * 0.15;
        b = c * 1.11;
        pinMode(BUZZER, OUTPUT);
    };
};
//=========================================================================

//*************************************************************************
// 2016-11-02
// Description:
//
//-------------------------------------------------------------------------

/*
 * Make a sound from buzzer
 */
void make_sound(unsigned short freq, unsigned char period) {

    unsigned short count;

    for (count = 1; count < period; count++) {
        timing_udelay(freq);
        digitalWrite(BUZZER, HIGH);
        timing_udelay(freq);
        digitalWrite(BUZZER, LOW);
    };
};

/*
 * Draw image on screen
 */
static inline bool collision_timedout(uint32_t from, uint32_t nr_ms) {
    return (millis() - from) > nr_ms;
}

/*
 * Draw image on screen
 */
void drawImage(int16_t x, int16_t y, const unsigned char *bitmap, int16_t w, int16_t h) {
    display.fillRect(x, y, w, h, BLACK);
    display.drawBitmap(x, y, bitmap, w, h, WHITE);
    display.display();
}

/*
 * Setup display text size and color
 */
void setTextDisplay() {
    display.setTextSize(1);
    display.setTextColor(WHITE);
}

/*
 * Display Ericsson Logo
 */
void displayLogo() {
    drawImage(0, 0, ericsson_logo, 128, 64);
    delay(1500);
}

/*
 * Display message
 */
void displayMessage(uint16_t delay_time, char *text, const unsigned char *bitmap, uint8_t w, uint8_t h) {
    clear_OLED_terminal();

    displayAntenna();
    displayBattery(battery_header);

    setTextDisplay();
    display.setCursor(0, 38);
    display.println(text);
    drawImage(52, 8, bitmap, w, h);
    delay(delay_time);
}

/*
 * Depending on battery level - display battery icon
 */
void displayBattery(uint16_t battery) {
    if (battery >= 300) {
        drawImage(115, 0, batteries[3], 16, 8);
    } else if (battery >= 200 && battery < 300) {
        drawImage(115, 0, batteries[2], 16, 8);
    } else if (battery >= 100 && battery < 200) {
        drawImage(115, 0, batteries[1], 16, 8);
    } else {
        drawImage(115, 0, batteries[0], 16, 8);
    }
}

/*
 * Depending on rssi level - display antenna icon
 */
void displayAntenna() {
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("Antenna ");
    DEBUG_STREAM.println(rssi);
#endif
    if (rssi >= 0) {

        if (rssi >= 20) {
            drawImage(-2, -2, antennas[3], 16, 8);
        } else if (rssi >= 15 && rssi < 20) {
            drawImage(-2, -2, antennas[2], 16, 8);
        } else if (rssi >= 10 && rssi < 15) {
            drawImage(-2, -2, antennas[1], 16, 8);
        } else {
            drawImage(-2, -2, antennas[0], 16, 8);
        }
    } else {
        if (rssi >= -73) {
            drawImage(-2, -2, antennas[3], 16, 8);
        } else if (rssi <= -74 && rssi > -84) {
            drawImage(-2, -2, antennas[2], 16, 8);
        } else if (rssi <= -84 && rssi > -94) {
            drawImage(-2, -2, antennas[1], 16, 8);
        } else {
            drawImage(-2, -2, antennas[0], 16, 8);
        }
    }
}


/*
 * Display connected status
 */
void displayConnected() {
    display.fillRect(39, 12, 50, 40, BLACK);
    display.fillRect(0, 47, 128, 20, BLACK);
    drawImage(50, 17, icon_ok, 32, 24);

    display.setCursor(38, 52);
    display.println(connected);
    display.display();
    delay(3000);
}

/*
 * Display icons when cube connect to network
 */
bool displayLoading() {
    display.fillRect(39, 12, 50, 40, BLACK);
    display.fillRect(0, 47, 128, 20, BLACK);
    drawImage(5, 17, cube, 24, 24);
    drawImage(94, 12, tower, 32, 32);

    setTextDisplay();

    if (reconnection_times >= 2) {
        drawImage(52, 17, icon_cross, 24, 24);
        display.setCursor(0, 47);
        display.println(error_connection);
        display.display();
        delay(4000);
        return false;
    } else if (reconnection_times > 0) {
        drawImage(52, 17, icon_cross, 24, 24);
        display.setCursor(0, 47);
        display.println(retrying_connect);
        display.display();
        delay(2500);
    }
    display.fillRect(39, 12, 50, 40, BLACK);
    display.fillRect(0, 47, 128, 20, BLACK);
    display.setCursor(0, 47);
    display.println(connecting);
    display.display();
    return true;
}


/*
 * Clear Display
 */
void clearDisplay() {
    display.fillRect(0, 10, 128, 64, BLACK);
    display.display();
    drawImage(-5, 15, bigCube, 40, 40);
}



void displayMode() {
    display.fillRect(38, 18, 100, 8, BLACK);
    display.setCursor(38, 18);
    display.setTextColor(WHITE);
    display.print(menus[menu_cursor]);
    display.display();
}

/*
 * Display Cube status (Idle or Sending data)
 */
void displayStatus(uint8_t statusType) {
    display.fillRect(38, 40, 100, 24, BLACK);
    display.setCursor(38, 40);
    display.setTextColor(WHITE);
    display.print(status[statusType]);
    display.display();

    display.setTextColor(WHITE);
    display.setCursor(94, 55);
    display.print(nbiot.getName());
    display.display();
}

/*
 * Display "Charging" if cable is plugged in
 */
void checkCharging() {
    if (analogRead(BATT_CHARGING_VALUE) > 300) {
        display.setCursor(65, 0);
        display.setTextColor(WHITE);
        display.print(charging);
        display.display();
    } else {
        display.fillRect(65, 0, 50, 10, BLACK);
        display.display();
    }
}

/*
 * Read rotary sensor and change mode (All data or medicine box)
 */
uint32_t readRotary() {
    sensorValue = analogRead(ROTARY_VALUE_PIN);
    if (sensorValue < 400) {
        menu_cursor = 0; //All data
    } else {
        menu_cursor = 1; // Medicine
    }
    displayMode();
    return sensorValue;
}

/*
 * Read Signal quality (RSI on dBm and BER)
 */
uint32_t readRSSI() {
    if (!nbiot.getRSSIAndBER(&rssi, &ber)) {
        rssi = -999;
        ber = 100;
    };
#ifdef DEBUG_LOG
    sprintf(tmpString, "RSSI= %d dBm", rssi);
    DEBUG_STREAM.println(tmpString);
#endif
    return rssi;
}


/*
 * Read battery value
 */
uint32_t readBattery() {
    //--- BATTERY value -------------------------------------
    sensorValue = analogRead(BATT_VOLTAGE_PIN);
#ifdef DEBUG_LOG
    sprintf(tmpString, "Batt= %d [V]", sensorValue);
    DEBUG_STREAM.println(tmpString);
#endif
    return sensorValue;
}

/*
 * Check button press
 */
int readWakeUpButton() {
    return digitalRead(WAKE_UP_PIN);
}

/*
 * Turn off power - to save battery. Go to deep sleep
 */
void waitResetLoop(bool resetModem) {

    if (resetModem) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("[Sleep mode] - prepare to sleep");
#endif
        //--- Switching off all the resources ----------------------
        if (nbiot.isConnected()) {
            nbiot.disconnect();
            delay(2000);
            digitalWrite(N2_ENABLE, LOW); // N2 -> OFF
        };
    }

    clear_OLED_terminal();
    digitalWrite(LED_BLUE, HIGH); // LED_BLUE -> OFF
    digitalWrite(LED_RED, HIGH);  // LED_RED -> OFF
    digitalWrite(BUZZER, LOW);    // BUZZER -> OFF

    //--- Disable VCC2 -------------------------------------------
    digitalWrite(VCC2_ENABLE, LOW); // VCC2 -> OFF

    //--- Disable VCC1 -------------------------------------------
    digitalWrite(VCC1_ENABLE, LOW); // VCC1 -> OFF
    //-----------------------------------------------------------

    //--- Disable Battery measurements ---------------------------
    digitalWrite(BATT_VOLTAGE_ENABLE, LOW); // BATT_ENABLE -> OFF
    //-----------------------------------------------------------

    delay(2000);

    while (1) {
        wake_up_state = readWakeUpButton();
        if (!wake_up_state) {
#ifdef DEBUG_LOG
            DEBUG_STREAM.println("[Wake up]");
#endif
            //--- time to wake up ----------
            NVIC_SystemReset(); // processor software reset
        };
    };
}

/*
 * Flash the led - indicator that data is sending
 */
void sendData() {
    displayStatus(1);
    isSendingData = true;
    digitalWrite(LED_BLUE, LOW);
}


/*
 * Function update sensor values of all registered objects
 */
void updateSensors(lwm2m_context_t *context, bool isStatusChanged, cube3_event_state_t ev_state) {
    double sensorValue = 0;

    //--- TEMPERATURE ----------------------------------------
    sensorValue = hts221.readTemperature();
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("TEMPERATURE ");
    DEBUG_STREAM.println(sensorValue);
#endif
    update_sensor_value(context, "/3303/0/5700", sensorValue);

    // --- LOUDNESS -------------------------------------------
    sensorValue = analogRead(SOUND_VALUE);
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("LOUDNESS ");
    DEBUG_STREAM.println(sensorValue);
#endif
    update_sensor_value(context, "/3324/0/5700", sensorValue);

    //--- HUMIDITY -------------------------------------------
    sensorValue = hts221.readHumidity();
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("HUMIDITY ");
    DEBUG_STREAM.println(sensorValue);
#endif
    update_sensor_value(context, "/3304/0/5700", sensorValue);
    //--------------------------------------------------------

    //--- PRESSURE -------------------------------------------
    sensorValue = lps22hb.readPressure();
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("PRESSURE ");
    DEBUG_STREAM.println(sensorValue);
#endif
    update_sensor_value(context, "/3323/0/5700", sensorValue);
    //--------------------------------------------------------

    //--- LUMINANCE ---------------------------------------
    sensorValue = analogRead(LIGHT_VALUE_PIN);
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("LUMINANCE ");
    DEBUG_STREAM.println(sensorValue);
#endif
    update_sensor_value(context, "/3301/0/5700", sensorValue);
    //-------------------------------------------------------

    //--- POWER ---------------------------------------
    sensorValue = readBattery();
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("POWER ");
    DEBUG_STREAM.println(sensorValue);
#endif
    update_sensor_value(context, "/3328/0/5700", sensorValue);
    //-------------------------------------------------------

    // --- FLAME ---------------------------------------
    sensorValue = analogRead(FLAME_VALUE_PIN);
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("FLAME ");
    DEBUG_STREAM.println(sensorValue);
#endif
    update_sensor_value(context, "/3202/0/5600", sensorValue);
    //-------------------------------------------------------

    // --- AIR_QUALITY -------------------------------------------
    sensorValue = analogRead(AIR_QUALITY);
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("AIR QUALITY ");
    DEBUG_STREAM.println(sensorValue);
#endif
    update_sensor_value(context, "/3202/1/5600", sensorValue);

    pinMode(COLLISION,
            INPUT);// Bug - if you don't do it, COLLISION sensor will return 0 any time (just when analogRead(AIR_QUALITY) )
    // --- COLLISION ---------------------------------------
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("COLLISION ");
    DEBUG_STREAM.println(ev_state==EVENT_COLLISION?1:0);
#endif
    update_event_value(context, "/3342/0/5500", ev_state == EVENT_COLLISION ? "1" : "0");
    //-------------------------------------------------------

    // --- PUSH_BUTTON ---------------------------------------
#ifdef DEBUG_LOG
    DEBUG_STREAM.print("PUSH_BUTTON ");
    DEBUG_STREAM.println(ev_state==EVENT_BUTTON?1:0);
#endif
    update_event_value(context, "/3347/0/5500", ev_state == EVENT_BUTTON ? "1" : "0");
    //-------------------------------------------------------

    if (isStatusChanged) {
        sendData();
    }
}

//********************************************************************************************************
// 2017-11-30
// SETUP
//--------------------------------------------------------------------------------------------------------
void setup() {
    delay(2000); // time required for USB port to reconfigure after upload

    //--- Serial definiions -------------------------------------
#ifdef DEBUG_LOG
    DEBUG_STREAM.begin(9600);
    Log.begin(LOG_LEVEL_VERBOSE, &DEBUG_STREAM);
#endif
    MODEM_STREAM.begin(9600);

    //--- GPIO init ---------------------------------------------
    pinMode(VCC1_ENABLE, OUTPUT);
    pinMode(VCC2_ENABLE, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(N2_ENABLE, OUTPUT);
    pinMode(RGB_LED_PIN, OUTPUT);
    pinMode(BUZZER, OUTPUT);

    pinMode(COLLISION, INPUT);
    pinMode(LIGHT_VALUE_PIN, INPUT);
    pinMode(ROTARY_VALUE_PIN, INPUT);
    pinMode(SOUND_VALUE, INPUT);
    pinMode(FLAME_VALUE_PIN, INPUT);
    pinMode(AIR_QUALITY, INPUT);
    pinMode(WAKE_UP_PIN, INPUT);
    pinMode(BATT_VOLTAGE_PIN, INPUT);
    pinMode(BATT_VOLTAGE_ENABLE, OUTPUT);
    pinMode(RELAY_IN1, OUTPUT);
    pinMode(RELAY_IN2, OUTPUT);

    digitalWrite(LED_BLUE, HIGH);           // LED_BLUE -> OFF
    digitalWrite(LED_RED, HIGH);            // LED_RED -> OFF
    digitalWrite(N2_ENABLE, LOW);           // VCC1 -> OFF
    digitalWrite(VCC1_ENABLE, LOW);         // VCC1 -> OFF
    digitalWrite(VCC2_ENABLE, LOW);         // VCC2 -> OFF
    digitalWrite(BUZZER, LOW);              // BUZZER -> OFF
    digitalWrite(BATT_VOLTAGE_ENABLE, LOW); // BATT_ENABLE -> OFF
    digitalWrite(RELAY_IN1, LOW);           // RELAY_IN1 -> OFF
    digitalWrite(RELAY_IN2, LOW);           // RELAY_IN2 -> OFF
};

//=== SETUP ============================================================================================

//*******************************************************************************************************
// M A I N   L O O P
//-------------------------------------------------------------------------------------------------------
void loop() {
    int result;
    int lifetime = 70;
    double latitude, longitude;
    const char *localPort = "4200";
    time_t reboot_time = 0;
    client_data_t data;
    lwm2m_context_t *lwm2mH = NULL;

#ifdef LWM2M_BOOTSTRAP
    lwm2m_client_state_t previousState = STATE_INITIAL;
#endif

    memset(&data, 0, sizeof(client_data_t));
    data.addressFamily = AF_INET;

    //-=-=-=-=-=-=-=-=
    MODEM_STREAM.begin(9600);
    delay(2000); // time required for USB port to reconfigure after upload

    //--- Variables init ----------------------------------------
    wake_up_state = 0;

    //--- Enable VCC1 -------------------------------------------
    digitalWrite(VCC1_ENABLE, HIGH); // VCC1 -> ON

    //--- Enable VCC2 -------------------------------------------
    digitalWrite(VCC2_ENABLE, HIGH); // VCC2 -> ON
    delay(1000);                     // Important !!!
    //-----------------------------------------------------------

    //--- I2C init ----------------------------------------------
    Wire.begin();
    Wire.setClock(100000); // 100 kHz

//--- OLED init ---------------------------------------------
#ifdef DEBUG_LOG
    DEBUG_STREAM.println("[Setup] Trying to init OLED...");
#endif
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize with the I2C addr 0x3D (for the 128x64)
    delay(2000);
    clear_OLED_terminal();

    displayLogo();

//--- HTS221 temp & hum sensor init --------------------------
#ifdef DEBUG_LOG
    DEBUG_STREAM.println("[Setup] Trying to init HTS221...");
#endif
    if (hts221.begin() == false) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("[Setup] Error while retrieving WHO_AM_I byte...");
#endif
        displayMessage(5000, hts221_error, icon_cross, 24, 24);
        waitResetLoop(false);
    };
    //------------------------------------------------------------

    //--- LPS221 pressure sensor init ----------------------------
    lps22hb.begin(0x5D);
#ifdef DEBUG_LOG
    DEBUG_STREAM.println("[Setup] Trying to init LPS221...");
#endif
    if (lps22hb.whoAmI() == false) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("[Setup] Error while retrieving WHO_AM_I byte...");
#endif
        displayMessage(5000, lps22hb_error, icon_cross, 24, 24);
        waitResetLoop(false);
    };

    //--- Enable Battery measurements ---------------------------
    digitalWrite(BATT_VOLTAGE_ENABLE, HIGH); // BATT_ENABLE -> ON
    //-----------------------------------------------------------

    //--- SD card init ----------------------------------------
    if (!SD.begin(4)) {
        displayMessage(5000, sd_card_failed, icon_cross, 24, 24);
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("[Setup] - error - no sd card");
        DEBUG_STREAM.println("[Setup] - error was shown");
#endif
        waitResetLoop(false);
    }
#ifdef DEBUG_LOG
    DEBUG_STREAM.println("[Setup] - sd card - initialization done");
#endif

    // re-open the file for reading:
    //read config.txt file with sequence of AT commands
    config = SD.open("config.txt");
    if (config) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("config.txt:");
#endif
        // read from the file until there's nothing else in it:
        StaticJsonBuffer<3072> jsonBuffer;
        // Parse the root object
        JsonObject &root = jsonBuffer.parseObject(config);

        if (!root.success()) {
#ifdef DEBUG_LOG
            DEBUG_STREAM.println(F("Failed to read file, using default configuration"));
#endif
            displayMessage(5000, error_config_file, icon_cross, 24, 24);
            waitResetLoop(false);
        }
        JsonArray &commands = root["commands"];
        int i = 0;
        commands_count = commands.size();
        commands_config = (ConfigJson *) malloc(sizeof(ConfigJson) * commands_count);
        //read commands for connecting
        for (auto &command : commands) {
            commands_config[i].command = (char *) malloc(strlen(command["command"]) + 1);
            strcpy(commands_config[i].command, command["command"]);
            commands_config[i].delay_time = command["delay"];
            commands_config[i].retries = command["retries"];
            commands_config[i].purge = command["purge"];
            i++;
        }
        config.close();
    } else {
// if the file wasn't opened, print an error:
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("[Setup] - error opening config.txt");
#endif
        displayMessage(5000, config_file_missing, icon_cross, 24, 24);
        waitResetLoop(false);
    }

    //read conn.txt
    //     {
    // 	    "id":"Cube", - name of the lwm2m client
    // 	    "address": "127.0.0.1", - server ip
    // 	    "port": 5683 - coap port of server
    // 	    "latitude": 52.3638418
    // 	    "longitude": 4.8942684
    //     }
    config = SD.open("conn.txt");
    if (config) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("conn.txt:");
#endif
        // read from the file until there's nothing else in it:
        StaticJsonBuffer<2048> jsonBuffer;
        // Parse the root object
        JsonObject &root = jsonBuffer.parseObject(config);

        if (!root.success()) {
#ifdef DEBUG_LOG
            DEBUG_STREAM.println(F("Failed to read file, using default configuration"));
#endif
            displayMessage(5000, error_connection_file, icon_cross, 24, 24);
            waitResetLoop(false);
        }
        latitude = root["latitude"];
        longitude = root["longitude"];

        if (strlen(root["id"]) == 0 || strlen(root["address"]) == 0 || strlen(root["port"]) == 0) {
#ifdef DEBUG_LOG
            DEBUG_STREAM.println("[Setup] - some fields in conn.txt file is empty");
#endif
            displayMessage(5000, config_file_fields_missing, icon_cross, 24, 24);
            waitResetLoop(false);
        }
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Init nbiot");
#endif
        nbiot.setName(root["id"]);
        nbiot.setServerIP(root["address"]);
        nbiot.setPort(root["port"]);
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("End init nbiot");
#endif
        config.close();
    } else {
#ifdef DEBUG_LOG
        // if the file didn't open, print an error:
        DEBUG_STREAM.println("[Setup] - error opening conn.txt");
#endif
        displayMessage(5000, config_file_missing, icon_cross, 24, 24);
        waitResetLoop(false);
    }

    //-----------------------------------------------------------
    digitalWrite(RELAY_IN1, LOW);
    digitalWrite(RELAY_IN2, LOW);
    digitalWrite(RELAY_IN1, HIGH);
    delay(500);
    digitalWrite(RELAY_IN1, LOW);
    delay(500);
    digitalWrite(RELAY_IN2, HIGH);
    delay(500);
    digitalWrite(RELAY_IN2, LOW);

    // --- Enable N2 power ---------------------------------------
    // println_on_OLED("uBLOX init...");
    digitalWrite(N2_ENABLE, HIGH);
#ifdef DEBUG_LOG
    DEBUG_STREAM.println("[Setup] N2 enabled");
#endif

    //--- Init UBLOX NB-IoT module ------------------------------
    nbiot.init(MODEM_STREAM, N2_ENABLE);
#ifdef DEBUG_LOG
    nbiot.setDiag(DEBUG_STREAM);
#endif

    delay(2000);

    nb_connect:
    clear_OLED_terminal();
    displayBattery(350);
    displayAntenna();

    if (!displayLoading()) {
        waitResetLoop(true);
    }
    char serverUri[50];
    sprintf(serverUri, "coap://%s:%d", nbiot.getServerIP(),
            nbiot.getPort()); //here you can change ip and port of server
#ifdef DEBUG_LOG
    DEBUG_STREAM.println(serverUri);
    DEBUG_STREAM.println(nbiot.getName());
    // --- Connect to Mobile Operator -----------------------------------
    DEBUG_STREAM.println("[Setup] Connecting to Mobile operator...");
#endif
    if (nbiot.connect(display, commands_config, commands_count)) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("[Setup] Connected succesfully!");
#endif
        make_sound(1000, 30);
        make_sound(500, 60);
        make_sound(250, 120);
        reconnection_times = 0;
    } else {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to connect!");
#endif
        make_sound(500, 120);
        make_sound(250, 60);
        make_sound(1000, 30);
        reconnection_times++;
        delay(1500);
        goto nb_connect;
        //===============================
    }

    open_socket:

#ifdef DEBUG_LOG
    DEBUG_STREAM.print("Trying to bind LWM2M Client to port ");
    DEBUG_STREAM.println(localPort);
#endif

    data.sock = nbiot.create_socket_s(localPort);
    if (data.sock < 0) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.print("Failed to open socket: ");
        DEBUG_STREAM.print(errno);
        DEBUG_STREAM.print(" ");
        DEBUG_STREAM.println(strerror(errno));
#endif
        displayMessage(3000, error_socket_create, icon_cross, 24, 24);
        goto open_socket;
    }
    displayConnected();
    update_header_counter = 30;

/*
 * Now the main function fill an array with each object, this list will be later passed to liblwm2m.
 * Those functions are located in their respective object file.
 */

    init_lwm2m_object:

    int serverId = 123;
#ifdef DEBUG_LOG
    DEBUG_STREAM.println(serverUri);
#endif
    objArray[0] = get_security_object(serverId, serverUri, NULL, NULL, NULL, false);

    if (NULL == objArray[0]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create security object\r\n");
#endif
    }
    data.securityObjP = objArray[0];
    objArray[1] = get_server_object(serverId, "U", lifetime, false);
    if (NULL == objArray[1]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create server object\r\n");
#endif
    }
    objArray[2] = get_object_device();
    if (NULL == objArray[2]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create Device object\r\n");
#endif
    }
    objArray[3] = get_object_firmware();
    if (NULL == objArray[3]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create Firmware object\r\n");
#endif
    }
    objArray[4] = get_object_location(latitude, longitude);
    if (NULL == objArray[4]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create location object\r\n");
#endif
    }
    objArray[5] = init_temperature_object();
    if (NULL == objArray[5]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create temp object\r\n");
#endif
    }
    objArray[6] = get_object_conn_m();
    if (NULL == objArray[6]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create connectivity monitoring object\r\n");
#endif
    }
    objArray[7] = get_object_conn_s();
    if (NULL == objArray[7]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create connectivity statistics object\r\n");
#endif
    }
    int instId = 0;
    objArray[8] = acc_ctrl_create_object();
    if (NULL == objArray[8]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create Access Control object\r\n");
#endif
    } else if (acc_ctrl_obj_add_inst(objArray[8], instId, 3, 0, serverId) == false) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create Access Control object instance\r\n");
#endif
    } else if (acc_ctrl_oi_add_ac_val(objArray[8], instId, 0, 0b000000000001111) == false) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create Access Control ACL default resource\r\n");
#endif
    } else if (acc_ctrl_oi_add_ac_val(objArray[8], instId, 999, 0b000000000000001) == false) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create Access Control ACL resource for serverId: 999\r\n");
#endif
    }
    objArray[9] = init_humidity_object();
    if (NULL == objArray[9]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create humidity object\r\n");
#endif
    }
    objArray[10] = init_luminance_object();
    if (NULL == objArray[10]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create luminance object\r\n");
#endif
    }
    objArray[11] = init_loudness_object();
    if (NULL == objArray[11]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create loudness object\r\n");
#endif
    }
    objArray[12] = init_power_object();
    if (NULL == objArray[12]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create power object\r\n");
#endif
    }
    objArray[13] = init_pressure_object();
    if (NULL == objArray[13]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create pressure object\r\n");
#endif
    }

    objArray[14] = init_flame_object();

    if (NULL == objArray[14]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create flame object\r\n");
#endif
    }

    objArray[15] = init_collision_object();

    if (NULL == objArray[15]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create collision object\r\n");
#endif
    }

    objArray[16] = init_push_button_object();

    if (NULL == objArray[16]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create push button object\r\n");
#endif
    }

    objArray[17] = init_air_object();

    if (NULL == objArray[17]) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("Failed to create air object\r\n");
#endif
    }

/*
 * The liblwm2m library is now initialized with the functions that will be in
 * charge of communication
 */
    lwm2mH = lwm2m_init(&data);
    if (NULL == lwm2mH) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.println("lwm2m_init() failed\r\n");
#endif
    }

#ifdef WITH_TINYDTLS
    data.lwm2mH = lwm2mH;
#endif
/*
 * We configure the liblwm2m library with the name of the client - which shall be unique for each client -
 * the number of objects we will be passing through and the objects array
 */
    result = lwm2m_configure(lwm2mH, nbiot.getName(), NULL, NULL, OBJ_COUNT, objArray); //here you can change name
    if (result != 0) {
#ifdef DEBUG_LOG
        DEBUG_STREAM.print("lwm2m_configure() failed: ");
        DEBUG_STREAM.println(result);
#endif
    }

    signal(SIGINT, handle_sigint);
/*
* Initialize value changed callback.
*/
    updateSensors(lwm2mH, false, NO_EVENT);

#ifdef DEBUG_LOG
    DEBUG_STREAM.print("LWM2M Client ");
    DEBUG_STREAM.print(nbiot.getName());
    DEBUG_STREAM.print(" started on port ");
    DEBUG_STREAM.println(localPort);
    DEBUG_STREAM.print("> ");
#endif

    //clear and display mode of Cube
    clearDisplay();
    displayStatus(0);
    event_sending = lwm2m_gettime();
/*
 * We now enter in a while loop that will handle the communications from the server
 */
    while (0 == g_quit) {
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        //check Rotary sensor - changing mode
        readRotary();
        //check if cable is plugged in
        checkCharging();

        switch (menu_cursor) {
            case 0:
                //send data if you pressed button
                if (!readWakeUpButton()) {
                    updateSensors(lwm2mH, true, EVENT_BUTTON);
                    //or it will be sent automatically after 5 minutes
                } else if ((event_sending + 300) <= lwm2m_gettime()) {
                    updateSensors(lwm2mH, true, EVENT_5_MIN);
                    event_sending = lwm2m_gettime();
                }
                break;
            case 1:
                uint32_t start = millis();
                //if you shake cube in this mode - it will send the data
                while (!collision_timedout(start, 120)) {
                    if (digitalRead(COLLISION) == 0) {
                        updateSensors(lwm2mH, true, EVENT_COLLISION);
                    }
                }
                break;
        }

        //--- Check if need to send a message --------------------------
        if (update_header_counter >= 30) {
#ifdef DEBUG_LOG
            DEBUG_STREAM.println("[loop] - update header antenna and battery");
#endif
            //update antenna icon
            readRSSI();
            displayAntenna();
            //update battery icon
            battery_header = readBattery();
            displayBattery(battery_header);

            update_header_counter = 0;
        } else {
            update_header_counter++;
        }

        /*
         * This function does two things:
         *  - first it does the work needed by liblwm2m (eg. (re)sending some packets).
         *  - Secondly it adjusts the timeout value (default 60s) depending on the state of the transaction
         *    (eg. retransmission) and the time between the next operation
         */
        result = lwm2m_step(lwm2mH, &(tv.tv_sec));
#ifdef DEBUG_LOG
        DEBUG_STREAM.print(" -> State: ");
        switch (lwm2mH->state)
        {
        case STATE_INITIAL:
            DEBUG_STREAM.println("STATE_INITIAL\r\n");
            break;
        case STATE_BOOTSTRAP_REQUIRED:
            DEBUG_STREAM.println("STATE_BOOTSTRAP_REQUIRED\r\n");
            break;
        case STATE_BOOTSTRAPPING:
            DEBUG_STREAM.println("STATE_BOOTSTRAPPING\r\n");
            break;
        case STATE_REGISTER_REQUIRED:
            DEBUG_STREAM.println("STATE_REGISTER_REQUIRED\r\n");
            break;
        case STATE_REGISTERING:
            DEBUG_STREAM.println("STATE_REGISTERING\r\n");
            break;
        case STATE_READY:
            DEBUG_STREAM.println("STATE_READY\r\n");
            break;
        default:
            DEBUG_STREAM.println("Unknown...\r\n");
            break;
        }
#endif

        if (result != 0) {
#ifdef DEBUG_LOG
            DEBUG_STREAM.print("[Loop] - lwm2m_step() failed: ");
            DEBUG_STREAM.println(result);
#endif
            if (previousState == STATE_BOOTSTRAPPING) {
#ifdef WITH_LOGS
                DEBUG_STREAM.print("[BOOTSTRAP] restore security and server objects\r\n");
#endif
                prv_restore_objects(lwm2mH);
                lwm2mH->state = STATE_INITIAL;
            } else {
                if (nbiot.isConnected()) { //if cube lost connection
                    make_sound(500, 120);
                    make_sound(250, 60);
                    make_sound(1000, 30);
                    displayMessage(3000, error_lost_connection, icon_cross, 24, 24);
                    goto nb_connect; //reconnect
                }
            }
        }

#ifdef LWM2M_BOOTSTRAP
        update_bootstrap_info(&previousState, lwm2mH);
#endif
        char nsorf[MAX_PACKET_SIZE] = {0};
        //receive data on socket
        int pending_message_length = nbiot.recvfrom_s(nsorf, 512);
        result = pending_message_length;

        if (result < 0) {
            if (errno != EINTR) {
#ifdef DEBUG_LOG
                DEBUG_STREAM.print("[Loop] - Error in select(): ");
                DEBUG_STREAM.print(errno);
                DEBUG_STREAM.print(" ");
                DEBUG_STREAM.println(strerror(errno));
#endif
            }
        } else if (result > 0) {
            uint8_t buffer[MAX_PACKET_SIZE];
            int numBytes;
            struct sockaddr_storage addr;
            socklen_t addrLen;

            addrLen = sizeof(addr);
            /*
            * Retrieve the data received
            */
            numBytes = pending_message_length;
            memcpy(buffer, nsorf, MAX_PACKET_SIZE);

            if (0 > numBytes) {
#ifdef DEBUG_LOG
                DEBUG_STREAM.print("[Loop] - Error in recvfrom() ");
                DEBUG_STREAM.print(errno);
                DEBUG_STREAM.print(" ");
                DEBUG_STREAM.println(strerror(errno));
#endif
            } else if (0 < numBytes) {
                connection_t *connP;
#ifdef DEBUG_LOG
                DEBUG_STREAM.print(numBytes);
                DEBUG_STREAM.print(" bytes received from ");
                DEBUG_STREAM.print(nbiot.getServerIP());
                DEBUG_STREAM.print(" ");
                DEBUG_STREAM.println(nbiot.getPort());
#endif
                connP = data.connList;
                lwm2m_handle_packet(lwm2mH, buffer, numBytes, connP);
                conn_s_updateRxStatistic(objArray[7], numBytes, false);
            }
        }

        //turn of the led when data was sent
        if (isSendingData) {
            isSendingData = false;
            digitalWrite(LED_BLUE, HIGH);
            displayStatus(0);
            delay(500);
            displayMessage(3000, sending_successful, icon_ok, 32, 24);
            waitResetLoop(true);
        }
    }

    if (g_quit == 1) {
#ifdef LWM2M_BOOTSTRAP
        close_backup_object();
#endif
        lwm2m_close(lwm2mH);
    }
    nbiot.close_socket_s(0);
    connection_free(data.connList);

    clean_security_object(objArray[0]);
    lwm2m_free(objArray[0]);
    clean_server_object(objArray[1]);
    lwm2m_free(objArray[1]);
    free_object_device(objArray[2]);
    free_object_firmware(objArray[3]);
    free_object_location(objArray[4]);
    free_object_conn_m(objArray[6]);
    free_object_conn_s(objArray[7]);
    acl_ctrl_free_object(objArray[8]);

    waitResetLoop(true);
};
//=== END of LOOP ==============================================================================================
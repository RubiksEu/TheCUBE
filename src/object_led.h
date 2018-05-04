#pragma once

#include "wakaama_object_utils.h"
#include <HardwareSerial.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LWM2M_LIGHT_OBJECT_ID            3311

#define LWM2M_LIGHT_SHORT_OBJECT_ID      0

#define SENSOR_L_UNITS           5701
#define COLOUR_L                 5706
#define SENSOR_L_VALUE           5805
#define POWER_L_FACTOR           5820
#define ON_L_OFF                 5850
#define DIMMER_L                 5851
#define ON_TIME_L                5852

typedef struct _led_prv_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data (uint8_t test in this case)
     */
    struct _led_prv_instance_ *next; // matches lwm2m_list_t::next
    uint16_t shortID;                // matches lwm2m_list_t::id
    char *units;
    char colour[11];
    double cumulative_active_power;
    double power_factor;
    bool on_off;
    int64_t dimmer;
    int64_t on_time;
} led_instance_t;

lwm2m_object_t *init_led_object();
#ifdef __cplusplus
}
#endif
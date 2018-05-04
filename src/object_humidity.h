#pragma once

#include "wakaama_object_utils.h"
#include <HardwareSerial.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LWM2M_HUMIDITY_OBJECT_ID            3304

#define LWM2M_HUMIDITY_SHORT_OBJECT_ID      0

#define MIN_H_MEASURED_VALUE     5601
#define MAX_H_MEASURED_VALUE     5602
#define MIN_H_RANGE_VALUE        5603
#define MAX_H_RANGE_VALUE        5604
#define RESET_H_MEASURED_VALUE   5605
#define SENSOR_H_VALUE           5700
#define SENSOR_H_UNITS           5701

typedef struct _humidity_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data (uint8_t test in this case)
     */
    struct _humidity_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    double  min_measured_value;
    double  max_measured_value;
    double  min_range_value;
    double  max_range_value;
    double  sensor_value;
    char*   units;
} humidity_instance_t;

lwm2m_object_t * init_humidity_object();
#ifdef __cplusplus
}
#endif
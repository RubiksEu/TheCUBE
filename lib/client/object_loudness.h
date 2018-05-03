#ifdef __cplusplus
extern "C" {
#endif

#ifndef WAKAAMA_OBJECT_LOUDNESS_H
#define WAKAAMA_OBJECT_LOUDNESS_H

#include "liblwm2m.h"
#include "lwm2mclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LWM2M_LOUD_SHORT_OBJECT_ID      0

#define MIN_LOUD_MEASURED_VALUE     5601
#define MAX_LOUD_MEASURED_VALUE     5602
#define MIN_LOUD_RANGE_VALUE        5603
#define MAX_LOUD_RANGE_VALUE        5604
#define RESET_LOUD_MEASURED_VALUE   5605
#define SENSOR_LOUD_VALUE           5700
#define SENSOR_LOUD_UNITS           5701
#define APPLICATION_LOUD_TYPE       5750
#define CURRENT_LOUD_CALIBRATION    5821

typedef struct _loudness_prv_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data (uint8_t test in this case)
     */
    struct _loudness_prv_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    double  min_measured_value;
    double  max_measured_value;
    double  min_range_value;
    double  max_range_value;
    double  sensor_value;
    double  units;
    char  application_type [20];
    char  current_calibration [20];
} loudness_instance_t;

lwm2m_object_t * init_loudness_object();

#endif //WAKAAMA_OBJECT_LOUDNESS_H

#ifdef __cplusplus
}
#endif
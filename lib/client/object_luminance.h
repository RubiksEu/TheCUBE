#ifdef __cplusplus
extern "C" {
#endif

#ifndef WAKAAMA_OBJECT_LUMINANCE_H
#define WAKAAMA_OBJECT_LUMINANCE_H

#include "liblwm2m.h"
#include "lwm2mclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LWM2M_LUMINANCE_SHORT_OBJECT_ID      0

#define MIN_LUM_MEASURED_VALUE     5601
#define MAX_LUM_MEASURED_VALUE     5602
#define MIN_LUM_RANGE_VALUE        5603
#define MAX_LUM_RANGE_VALUE        5604
#define RESET_LUM_MEASURED_VALUE   5605
#define SENSOR_LUM_VALUE           5700
#define SENSOR_LUM_UNITS           5701

typedef struct _luminance_prv_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data (uint8_t test in this case)
     */
    struct _luminance_prv_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    double  sensor_value;
    double  min_measured_value;
    double  max_measured_value;
    double  min_range_value;
    double  max_range_value;
    char   units [20];
} luminance_instance_t;

lwm2m_object_t * init_luminance_object();

#endif //WAKAAMA_OBJECT_LUMINANCE_H

#ifdef __cplusplus
}
#endif
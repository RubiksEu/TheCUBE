#ifdef __cplusplus
extern "C" {
#endif

#ifndef WAKAAMA_OBJECT_AIR_QUALITY_H
#define WAKAAMA_OBJECT_AIR_QUALITY_H


#include "liblwm2m.h"
#include "lwm2mclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LWM2M_AIR_SHORT_OBJECT_ID      1

#define SENSOR_A_CURRENT_VALUE   5600
#define APPLICATION_A_TYPE       5750
#define MAX_A_MEASURED_VALUE     5602
#define MAX_A_RANGE_VALUE        5604
#define MIN_A_MEASURED_VALUE     5601
#define MIN_A_RANGE_VALUE        5603
#define RESET_A_MEASURED_VALUE   5605
#define SENSOR_A_TYPE            5751

typedef struct _air_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data
     */
    struct _air_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    double  current_value;
    char   app_type[20];
    double  max_measured_value;
    double  max_range_value;
    double  min_measured_value;
    double  min_range_value;
    char   type[20];
} air_instance_t;

lwm2m_object_t * init_air_object();

#endif //WAKAAMA_OBJECT_AIR_QUALITY_H
#ifdef __cplusplus
}
#endif
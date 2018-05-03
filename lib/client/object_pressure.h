#ifdef __cplusplus
extern "C" {
#endif

#ifndef WAKAAMA_OBJECT_PRESSURE_H
#define WAKAAMA_OBJECT_PRESSURE_H

#include "liblwm2m.h"
#include "lwm2mclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LWM2M_PRESSURE_SHORT_OBJECT_ID      0

#define MIN_PR_MEASURED_VALUE     5601
#define MAX_PR_MEASURED_VALUE     5602
#define MIN_PR_RANGE_VALUE        5603
#define MAX_PR_RANGE_VALUE        5604
#define RESET_PR_MEASURED_VALUE   5605
#define SENSOR_PR_VALUE           5700
#define SENSOR_PR_UNITS           5701
#define APPLICATION_PR_TYPE       5750
#define CURRENT_PR_CALIBRATION    5821

typedef struct _pressure_prv_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data
     */
    struct _pressure_prv_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    double  min_measured_value;
    double  max_measured_value;
    double  min_range_value;
    double  max_range_value;
    double  sensor_value;
    double  units;
    char  application_type [20];
    char  current_calibration [20];
} pressure_instance_t;

lwm2m_object_t * init_pressure_object();

#endif //WAKAAMA_OBJECT_PRESSURE_H

#ifdef __cplusplus
}
#endif
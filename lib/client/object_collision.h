#ifdef __cplusplus
extern "C" {
#endif

#ifndef WAKAAMA_OBJECT_COLLISION_H
#define WAKAAMA_OBJECT_COLLISION_H

#include "liblwm2m.h"
#include "lwm2mclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LWM2M_COLLISION_SHORT_OBJECT_ID      0

#define APPLICATION_C_TYPE       5750
#define DIGITAL_C_INPUT_STATE    5500
#define DIGITAL_C_INPUT_STATE_2  5501
#define OFF_C_TIME               5854
#define ON_C_TIME                5852

typedef struct _collision_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data
     */
    struct _collision_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    char   app_type[20];
    bool  digital_input;
    int64_t  digital_input_state;
    int64_t  off_time;
    int64_t  on_time;
} collision_instance_t;

lwm2m_object_t * init_collision_object();


#endif //WAKAAMA_OBJECT_COLLISION_H
#ifdef __cplusplus
}
#endif


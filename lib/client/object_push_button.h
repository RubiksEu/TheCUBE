#ifdef __cplusplus
extern "C" {
#endif

#ifndef WAKAAMA_OBJECT_PUSH_BUTTON_H
#define WAKAAMA_OBJECT_PUSH_BUTTON_H

#include "liblwm2m.h"
#include "lwm2mclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LWM2M_PUSH_BUTTON_SHORT_OBJECT_ID      0

#define APPLICATION_P_TYPE       5750
#define DIGITAL_P_INPUT_STATE    5500
#define DIGITAL_P_INPUT_STATE_2  5501

typedef struct _push_button_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data
     */
    struct _push_button_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    char   app_type[20];
    bool  digital_input;
    int  digital_input_state;
} push_button_instance_t;

lwm2m_object_t * init_push_button_object();

#endif //WAKAAMA_OBJECT_PUSH_BUTTON_H

#ifdef __cplusplus
}
#endif

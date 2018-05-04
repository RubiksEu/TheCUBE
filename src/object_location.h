#pragma once

#include "wakaama_object_utils.h"
#include <HardwareSerial.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LWM2M_LOCATION_OBJECT_ID            6

#define RES_M_LATITUDE     0
#define RES_M_LONGITUDE    1
#define RES_O_ALTITUDE     2
#define RES_O_RADIUS       3
#define RES_O_VELOCITY     4
#define RES_M_TIMESTAMP    5
#define RES_O_SPEED        6

#define HORIZONTAL_VELOCITY_WITH_UNCERTAINTY 2

#define VELOCITY_OCTETS                      5  // for HORIZONTAL_VELOCITY_WITH_UNCERTAINTY 

typedef struct
{
    float    latitude;
    float    longitude;
    float    altitude;
    float    radius;
    uint8_t  velocity   [VELOCITY_OCTETS];        //3GPP notation 1st step: HORIZONTAL_VELOCITY_WITH_UNCERTAINTY
    unsigned long timestamp;
    float    speed;
} location_data_t;

lwm2m_object_t *init_location_object();
#ifdef __cplusplus
}
#endif
//
// Created by emergency on 1/31/2018.
//

/*
 * Implements an object for testing purpose
 *
 *                  Multiple
 * Object |  ID   | Instances | Mandatoty |
 *  Test  | 31024 |    Yes    |    No     |
 *
 *  Resources:
 *              Supported    Multiple
 *  Name | ID | Operations | Instances | Mandatory |  Type   | Range | Units | Description |
 *  test |  1 |    R/W     |    No     |    Yes    | Integer | 0-255 |       |             |
 *  exec |  2 |     E      |    No     |    Yes    |         |       |       |             |
 *  dec  |  3 |    R/W     |    No     |    Yes    |  Float  |       |       |             |
 *
 */

#include <Arduino.h>
#include "object_location.h"

/**
implementation for all read-able resources
*/
static uint8_t prv_res2tlv(lwm2m_data_t *dataP,
                           location_data_t *locDataP)
{
    //-------------------------------------------------------------------- JH --
    uint8_t ret = COAP_205_CONTENT;
    switch (dataP->id) // location resourceId
    {
    case RES_M_LATITUDE:
        lwm2m_data_encode_float(locDataP->latitude, dataP);
        break;
    case RES_M_LONGITUDE:
        lwm2m_data_encode_float(locDataP->longitude, dataP);
        break;
    case RES_O_ALTITUDE:
        lwm2m_data_encode_float(locDataP->altitude, dataP);
        break;
    case RES_O_RADIUS:
        lwm2m_data_encode_float(locDataP->radius, dataP);
        break;
    case RES_O_VELOCITY:
        lwm2m_data_encode_string((const char *)locDataP->velocity, dataP);
        break;
    case RES_M_TIMESTAMP:
        lwm2m_data_encode_int(locDataP->timestamp, dataP);
        break;
    case RES_O_SPEED:
        lwm2m_data_encode_float(locDataP->speed, dataP);
        break;
    default:
        ret = COAP_404_NOT_FOUND;
        break;
    }

    return ret;
}

/**
  * Implementation (callback-) function of reading object resources. For whole
  * object, single resources or a sequence of resources
  * see 3GPP TS 23.032 V11.0.0(2012-09) page 23,24.
  * implemented for: HORIZONTAL_VELOCITY_WITH_UNCERTAINT
  * @param objInstId    in,     instances ID of the location object to read
  * @param numDataP     in/out, pointer to the number of resource to read. 0 is the
  *                             exception for all readable resource of object instance
  * @param tlvArrayP    in/out, TLV data sequence with initialized resource ID to read
  * @param objectP      in,     private location data structure
  */
static uint8_t prv_location_read(uint16_t objInstId,
                                 int *numDataP,
                                 lwm2m_data_t **tlvArrayP,
                                 lwm2m_object_t *objectP)
{
    //-------------------------------------------------------------------- JH --
    int i;
    uint8_t result = COAP_500_INTERNAL_SERVER_ERROR;
    location_data_t *locDataP = (location_data_t *)(objectP->userData);

    // defined as single instance object!
    if (objInstId != 0)
        return COAP_404_NOT_FOUND;

    if (*numDataP == 0) // full object, readable resources!
    {
        uint16_t readResIds[] = {
            RES_M_LATITUDE,
            RES_M_LONGITUDE,
            RES_O_ALTITUDE,
            RES_O_RADIUS,
            RES_O_VELOCITY,
            RES_M_TIMESTAMP,
            RES_O_SPEED}; // readable resources!

        *numDataP = sizeof(readResIds) / sizeof(uint16_t);
        *tlvArrayP = lwm2m_data_new(*numDataP);
        if (*tlvArrayP == NULL)
            return COAP_500_INTERNAL_SERVER_ERROR;

        // init readable resource id's
        for (i = 0; i < *numDataP; i++)
        {
            (*tlvArrayP)[i].id = readResIds[i];
        }
    }

    for (i = 0; i < *numDataP; i++)
    {
        result = prv_res2tlv((*tlvArrayP) + i, locDataP);
        if (result != COAP_205_CONTENT)
            break;
    }

    return result;
}

/**
  * Convenience function to set the velocity attributes.
  * see 3GPP TS 23.032 V11.0.0(2012-09) page 23,24.
  * implemented for: HORIZONTAL_VELOCITY_WITH_UNCERTAINTY
  * @param locationObj location object reference (to be casted!)
  * @param bearing          [Deg]  0 - 359    resolution: 1 degree
  * @param horizontalSpeed  [km/h] 1 - s^16-1 resolution: 1 km/h steps
  * @param speedUncertainty [km/h] 1-254      resolution: 1 km/h (255=undefined!)
  */
void location_setVelocity(lwm2m_object_t *locationObj,
                          uint16_t bearing,
                          uint16_t horizontalSpeed,
                          uint8_t speedUncertainty)
{
    //-------------------------------------------------------------------- JH --
    location_data_t *pData = (location_data_t *)locationObj->userData;
    pData->velocity[0] = HORIZONTAL_VELOCITY_WITH_UNCERTAINTY << 4;
    pData->velocity[0] = (bearing & 0x100) >> 8;
    pData->velocity[1] = (bearing & 0x0FF);
    pData->velocity[2] = horizontalSpeed >> 8;
    pData->velocity[3] = horizontalSpeed & 0xff;
    pData->velocity[4] = speedUncertainty;
}


lwm2m_object_t *init_location_object()
{
    //-------------------------------------------------------------------- JH --
    lwm2m_object_t *locationObj;

    locationObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));
    if (NULL != locationObj)
    {
        memset(locationObj, 0, sizeof(lwm2m_object_t));

        // It assigns its unique ID
        // The 6 is the standard ID for the optional object "Location".
        locationObj->objID = LWM2M_LOCATION_OBJECT_ID;

        // and its unique instance
        locationObj->instanceList = (lwm2m_list_t *)lwm2m_malloc(sizeof(lwm2m_list_t));
        if (NULL != locationObj->instanceList)
        {
            memset(locationObj->instanceList, 0, sizeof(lwm2m_list_t));
        }
        else
        {
            lwm2m_free(locationObj);
            return NULL;
        }

        // And the private function that will access the object.
        // Those function will be called when a read query is made by the server.
        // In fact the library don't need to know the resources of the object, only the server does.
        //
        locationObj->readFunc = prv_location_read;
        locationObj->userData = lwm2m_malloc(sizeof(location_data_t));

        // initialize private data structure containing the needed variables
        if (NULL != locationObj->userData)
        {
            location_data_t *data = (location_data_t *)locationObj->userData;
            data->latitude = 27.986065; 
            data->longitude = 86.922623;
            data->altitude = 8495.0000;
            data->radius = 0.0;
            location_setVelocity(locationObj, 0, 0, 255); // 255: speedUncertainty not supported!
            data->timestamp = time(NULL);
            data->speed = 0.0;
        }
        else
        {
            lwm2m_free(locationObj);
            locationObj = NULL;
        }
    }

    return locationObj;
}

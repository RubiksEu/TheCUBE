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
#include <Adafruit_NeoPixel.h>
#include "object_led.h"
/*
 * Multiple instance objects can use userdata to store data that will be shared between the different instances.
 * The lwm2m_object_t object structure - which represent every object of the liblwm2m as seen in the single instance
 * object - contain a chained list called instanceList with the object specific structure prv_instance_t:
 */


#define RGB_LED_PIN 15

extern Adafruit_NeoPixel pixels;

void setColor(int led, int redValue, int greenValue, int blueValue)
{
    pixels.setPixelColor(led, pixels.Color(redValue, greenValue, blueValue));
    pixels.show();
}

static uint8_t prv_write(uint16_t instanceId,
                         int numData,
                         lwm2m_data_t *dataArray,
                         lwm2m_object_t *objectP)
{
    led_instance_t *targetP;
    int i;
    uint32_t r, g, b;
    targetP = (led_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
        return COAP_404_NOT_FOUND;

    for (i = 0; i < numData; i++)
    {
        switch (dataArray[i].id)
        {
        case SENSOR_L_UNITS:
            return COAP_404_NOT_FOUND;
        case COLOUR_L:
            strncpy((targetP)->colour, (char *)dataArray[i].value.asBuffer.buffer,
                    dataArray[i].value.asBuffer.length);
            (targetP)->colour[dataArray[i].value.asBuffer.length] = 0;
            sscanf((targetP)->colour, "%u %u %u", &r, &g, &b);
            if (r > 255)
            {
                r = 255;
            }
            if (g > 255)
            {
                g = 255;
            }
            if (b > 255)
            {
                b = 255;
            }
            sprintf((targetP->colour), "%d %d %d", r, g, b);
            setColor(0, r, g, b);
            setColor(1, r, g, b);
            break;
        case SENSOR_L_VALUE:
            if (1 != lwm2m_data_decode_float(dataArray + i, &(targetP->cumulative_active_power)))
            {
                return COAP_400_BAD_REQUEST;
            }
            break;
        case POWER_L_FACTOR:
            if (1 != lwm2m_data_decode_float(dataArray + i, &(targetP->power_factor)))
            {
                return COAP_400_BAD_REQUEST;
            }
            break;
        case ON_L_OFF:
            if (1 != lwm2m_data_decode_bool(dataArray + i, &(targetP->on_off)))
            {
                return COAP_400_BAD_REQUEST;
            }
            if (targetP->on_off)
            {
                setColor(0, 255, 255, 255);
                setColor(1, 255, 255, 255);
            }
            else
            {
                setColor(0, 0, 0, 0);
                setColor(1, 0, 0, 0);
            }
            break;
        case DIMMER_L:
            // if (checkDimmer(dataArray + i)) {
            if (1 != lwm2m_data_decode_int(dataArray + i, &(targetP->dimmer)))
            {

                return COAP_400_BAD_REQUEST;
            }
            // } else {
            // return COAP_402_BAD_OPTION;
            // }
            break;
        case ON_TIME_L:
            if (1 != lwm2m_data_decode_int(dataArray + i, &(targetP->on_time)))
            {
                return COAP_400_BAD_REQUEST;
            }
            break;
        default:
            return COAP_404_NOT_FOUND;
        }
    }

    return COAP_204_CHANGED;
}

static uint8_t prv_read(uint16_t instanceId,
                        int *numDataP,
                        lwm2m_data_t **dataArrayP,
                        lwm2m_object_t *objectP)
{
    led_instance_t *targetP;
    int i;

    targetP = (led_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
        return COAP_404_NOT_FOUND;

    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(7);
        if (*dataArrayP == NULL)
            return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 7;
        (*dataArrayP)[0].id = SENSOR_L_UNITS;
        (*dataArrayP)[1].id = COLOUR_L;
        (*dataArrayP)[2].id = SENSOR_L_VALUE;
        (*dataArrayP)[3].id = POWER_L_FACTOR;
        (*dataArrayP)[4].id = ON_L_OFF;
        (*dataArrayP)[5].id = DIMMER_L;
        (*dataArrayP)[6].id = ON_TIME_L;
    }

    for (i = 0; i < *numDataP; i++)
    {
        switch ((*dataArrayP)[i].id)
        {
        case SENSOR_L_UNITS:
            lwm2m_data_encode_string(targetP->units, *dataArrayP + i);
            break;
        case COLOUR_L:
            lwm2m_data_encode_string(targetP->colour, *dataArrayP + i);
            break;
        case SENSOR_L_VALUE:
            lwm2m_data_encode_float(targetP->cumulative_active_power, *dataArrayP + i);
            break;
        case POWER_L_FACTOR:
            lwm2m_data_encode_float(targetP->power_factor, *dataArrayP + i);
            break;
        case ON_L_OFF:
            lwm2m_data_encode_bool(targetP->on_off, *dataArrayP + i);
            break;
        case DIMMER_L:
            lwm2m_data_encode_int(targetP->dimmer, *dataArrayP + i);
            break;
        case ON_TIME_L:
            lwm2m_data_encode_int(targetP->on_time, *dataArrayP + i);
            break;
        default:
            return COAP_404_NOT_FOUND;
        }
    }

    return COAP_205_CONTENT;
}

lwm2m_object_t *init_led_object()
{
    lwm2m_object_t *testObj;

    testObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != testObj)
    {
        int i;
        led_instance_t *targetP;

        memset(testObj, 0, sizeof(lwm2m_object_t));

        testObj->objID = LWM2M_LIGHT_OBJECT_ID;
        for (i = 0; i < 1; i++)
        {
            targetP = (led_instance_t *)lwm2m_malloc(sizeof(led_instance_t));
            if (NULL == targetP)
                return NULL;
            memset(targetP, 0, sizeof(led_instance_t));
            targetP->shortID = LWM2M_LIGHT_SHORT_OBJECT_ID;
            targetP->units = "%";
            strcpy(targetP->colour, "0 0 0");
            targetP->cumulative_active_power = 0;
            targetP->power_factor = 0;
            targetP->on_off = false;
            targetP->dimmer = 0;
            targetP->on_time = 0;
            testObj->instanceList = LWM2M_LIST_ADD(testObj->instanceList, targetP);
        }
        /*
         * From a single instance object, two more functions are available.
         * - The first one (createFunc) create a new instance and filled it with the provided informations. If an ID is
         *   provided a check is done for verifying his disponibility, or a new one is generated.
         * - The other one (deleteFunc) delete an instance by removing it from the instance list (and freeing the memory
         *   allocated to it)
         */
        testObj->readFunc = prv_read;
        testObj->writeFunc = prv_write;
    }

    return testObj;
}

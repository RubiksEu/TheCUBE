#include "object_pressure.h"

static uint8_t prv_write(uint16_t instanceId,
                         int numData,
                         lwm2m_data_t * dataArray,
                         lwm2m_object_t * objectP)
{
    pressure_instance_t * targetP;
    int i;

    targetP = (pressure_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    for (i = 0 ; i < numData ; i++)
    {
        switch (dataArray[i].id)
        {
            case SENSOR_PR_VALUE:
                if (1 != lwm2m_data_decode_float(dataArray + i, &(targetP->sensor_value)))
                {
                    return COAP_400_BAD_REQUEST;
                }
                break;
            case APPLICATION_PR_TYPE:
                strncpy((targetP)->application_type, (char *)dataArray[i].value.asBuffer.buffer,
                        dataArray[i].value.asBuffer.length);
                (targetP)->application_type[dataArray[i].value.asBuffer.length] = 0;
                break;
            case CURRENT_PR_CALIBRATION:
                strncpy((targetP)->current_calibration, (char *)dataArray[i].value.asBuffer.buffer,
                        dataArray[i].value.asBuffer.length);
                (targetP)->current_calibration[dataArray[i].value.asBuffer.length] = 0;
                break;
            default:
                return COAP_404_NOT_FOUND;
        }
    }

    return COAP_204_CHANGED;
}

static uint8_t prv_read(uint16_t instanceId,
                        int * numDataP,
                        lwm2m_data_t ** dataArrayP,
                        lwm2m_object_t * objectP)
{
    pressure_instance_t * targetP;
    int i;

    targetP = (pressure_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(8);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 8;
        (*dataArrayP)[0].id = MIN_PR_MEASURED_VALUE;
        (*dataArrayP)[1].id = MAX_PR_MEASURED_VALUE;
        (*dataArrayP)[2].id = MIN_PR_RANGE_VALUE;
        (*dataArrayP)[3].id = MAX_PR_RANGE_VALUE;
        (*dataArrayP)[4].id = SENSOR_PR_VALUE;
        (*dataArrayP)[5].id = SENSOR_PR_UNITS;
        (*dataArrayP)[6].id = APPLICATION_PR_TYPE;
        (*dataArrayP)[7].id = CURRENT_PR_CALIBRATION;
    }
    for (i = 0 ; i < *numDataP ; i++)
    {
        switch ((*dataArrayP)[i].id)
        {
            case MIN_PR_MEASURED_VALUE:
                lwm2m_data_encode_float((double)targetP->min_measured_value, *dataArrayP + i);
                break;
            case MAX_PR_MEASURED_VALUE:
                lwm2m_data_encode_float((double)targetP->max_measured_value, *dataArrayP + i);
                break;
            case MIN_PR_RANGE_VALUE:
                lwm2m_data_encode_float((double)targetP->min_range_value, *dataArrayP + i);
                break;
            case MAX_PR_RANGE_VALUE:
                lwm2m_data_encode_float((double)targetP->max_range_value, *dataArrayP + i);
                break;
            case SENSOR_PR_VALUE:
                lwm2m_data_encode_float((double)targetP->sensor_value, *dataArrayP + i);
                break;
            case SENSOR_PR_UNITS:
                lwm2m_data_encode_float((double)targetP->units, *dataArrayP + i);
                break;
            case APPLICATION_PR_TYPE:
                lwm2m_data_encode_string(targetP->application_type,*dataArrayP + i);
                break;
            case CURRENT_PR_CALIBRATION:
                lwm2m_data_encode_string(targetP->current_calibration,*dataArrayP + i);
                break;
            default:
                return COAP_404_NOT_FOUND;
        }
    }

    return COAP_205_CONTENT;
}

static uint8_t prv_exec(uint16_t instanceId,
                        uint16_t resourceId,
                        uint8_t * buffer,
                        int length,
                        lwm2m_object_t * objectP)
{

    if (NULL == lwm2m_list_find(objectP->instanceList, instanceId)) return COAP_404_NOT_FOUND;

    switch (resourceId)
    {
        case RESET_PR_MEASURED_VALUE:
            return COAP_204_CHANGED;
        default:
            return COAP_404_NOT_FOUND;
    }
}

lwm2m_object_t * init_pressure_object()
{
    lwm2m_object_t * testObj;

    testObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != testObj)
    {
        int i;
        pressure_instance_t * targetP;

        memset(testObj, 0, sizeof(lwm2m_object_t));

        testObj->objID = PRESSURE_OBJECT_ID;
        for (i=0 ; i < 1 ; i++)
        {
            targetP = (pressure_instance_t *)lwm2m_malloc(sizeof(pressure_instance_t));
            if (NULL == targetP) return NULL;
            memset(targetP, 0, sizeof(pressure_instance_t));
            targetP->shortID = LWM2M_PRESSURE_SHORT_OBJECT_ID;
            targetP->min_measured_value = 0;
            targetP->max_measured_value = 0;
            targetP->min_range_value = 0;
            targetP->max_range_value = 0;
            targetP->sensor_value = 0;
            targetP->units = 0;
            strcpy(targetP->application_type,"type");
            strcpy(targetP->current_calibration, "normal");
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
        testObj->executeFunc = prv_exec;
    }

    return testObj;
}


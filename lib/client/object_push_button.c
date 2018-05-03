#include "object_push_button.h"

static uint8_t prv_write(uint16_t instanceId,
                         int numData,
                         lwm2m_data_t * dataArray,
                         lwm2m_object_t * objectP)
{
    push_button_instance_t * targetP;
    int i;

    targetP = (push_button_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    for (i = 0 ; i < numData ; i++)
    {
        switch (dataArray[i].id)
        {

            case DIGITAL_P_INPUT_STATE:
                if (1 != lwm2m_data_decode_bool(dataArray + i, &(targetP->digital_input)))
                {
                    return COAP_400_BAD_REQUEST;
                }
                break;
            case APPLICATION_P_TYPE:
                strncpy((targetP)->app_type, (char *)dataArray[i].value.asBuffer.buffer,
                        dataArray[i].value.asBuffer.length);
                (targetP)->app_type[dataArray[i].value.asBuffer.length] = 0;
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
    push_button_instance_t * targetP;
    int i;

    targetP = (push_button_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(3);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 3;
        (*dataArrayP)[0].id = APPLICATION_P_TYPE;
        (*dataArrayP)[1].id = DIGITAL_P_INPUT_STATE;
        (*dataArrayP)[2].id = DIGITAL_P_INPUT_STATE_2;
    }
    for (i = 0 ; i < *numDataP ; i++)
    {
        switch ((*dataArrayP)[i].id)
        {
            case DIGITAL_P_INPUT_STATE:
                lwm2m_data_encode_bool(targetP->digital_input, *dataArrayP + i);
                break;
            case DIGITAL_P_INPUT_STATE_2:
                lwm2m_data_encode_int(targetP->digital_input_state, *dataArrayP + i);
                break;
            case APPLICATION_P_TYPE:
                lwm2m_data_encode_string(targetP->app_type,*dataArrayP + i);
                break;
            default:
                return COAP_404_NOT_FOUND;
        }
    }

    return COAP_205_CONTENT;
}

lwm2m_object_t * init_push_button_object()
{
    lwm2m_object_t * testObj;

    testObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != testObj)
    {
        int i;
        push_button_instance_t * targetP;

        memset(testObj, 0, sizeof(lwm2m_object_t));

        testObj->objID = PUSH_BUTTON_OBJECT_ID;
        for (i=0 ; i < 1 ; i++)
        {
            targetP = (push_button_instance_t *)lwm2m_malloc(sizeof(push_button_instance_t));
            if (NULL == targetP) return NULL;
            memset(targetP, 0, sizeof(push_button_instance_t));
            targetP->shortID = LWM2M_COLLISION_SHORT_OBJECT_ID;
            targetP->digital_input = false;
            targetP->digital_input_state = 0;
            strcpy(targetP->app_type,"type");
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


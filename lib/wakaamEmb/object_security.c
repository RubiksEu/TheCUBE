/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *
 *******************************************************************************/

/*
 *  Resources:
 *
 *          Name            | ID | Operations | Instances | Mandatory |  Type   |  Range  | Units |
 *  Server URI              |  0 |            |  Single   |    Yes    | String  |         |       |
 *  Bootstrap Server        |  1 |            |  Single   |    Yes    | Boolean |         |       |
 *  Security Mode           |  2 |            |  Single   |    Yes    | Integer |   0-3   |       |
 *  Public Key or ID        |  3 |            |  Single   |    Yes    | Opaque  |         |       |
 *  Server Public Key or ID |  4 |            |  Single   |    Yes    | Opaque  |         |       |
 *  Secret Key              |  5 |            |  Single   |    Yes    | Opaque  |         |       |
 *  SMS Security Mode       |  6 |            |  Single   |    Yes    | Integer |  0-255  |       |
 *  SMS Binding Key Param.  |  7 |            |  Single   |    Yes    | Opaque  |   6 B   |       |
 *  SMS Binding Secret Keys |  8 |            |  Single   |    Yes    | Opaque  | 32-48 B |       |
 *  Server SMS Number       |  9 |            |  Single   |    Yes    | Integer |         |       |
 *  Short Server ID         | 10 |            |  Single   |    No     | Integer | 1-65535 |       |
 *  Client Hold Off Time    | 11 |            |  Single   |    Yes    | Integer |         |   s   |
 *
 */

/*
 * Here we implement a very basic LWM2M Security Object which only knows NoSec security mode.
 */

#include "wakaama/liblwm2m.h"
#include "wakaama_client_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

lwm2m_object_t security_object;
static uint8_t prv_get_value(lwm2m_data_t * dataP,
                             security_instance_t * targetP)
{
    switch (dataP->id)
    {
    case LWM2M_SECURITY_URI_ID:
        lwm2m_data_encode_string(targetP->uri, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_BOOTSTRAP_ID:
        lwm2m_data_encode_bool(targetP->isBootstrap, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SECURITY_ID:
        lwm2m_data_encode_int(targetP->securityMode, dataP);
        return COAP_205_CONTENT;

#ifdef LWM2M_WITH_DTLS
    case LWM2M_SECURITY_PUBLIC_KEY_ID:
        lwm2m_data_encode_opaque((uint8_t*)targetP->publicIdentity, (size_t)targetP->publicIdLen, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SERVER_PUBLIC_KEY_ID:
        lwm2m_data_encode_opaque((uint8_t*)targetP->serverPublicKey, (size_t)targetP->serverPublicKeyLen, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SECRET_KEY_ID:
        lwm2m_data_encode_opaque((uint8_t*)targetP->secretKey, (size_t)targetP->secretKeyLen, dataP);
        return COAP_205_CONTENT;
#endif
#ifdef LWM2M_WITH_SMS
    case LWM2M_SECURITY_SMS_SECURITY_ID:
        lwm2m_data_encode_int(targetP->smsSecurityMode, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SMS_KEY_PARAM_ID:
        lwm2m_data_encode_opaque(targetP->smsParams, targetP->smsParamsLen, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SMS_SECRET_KEY_ID:
        lwm2m_data_encode_opaque(targetP->smsSecret, targetP->smsSecretLen, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_SMS_SERVER_NUMBER_ID:
        lwm2m_data_encode_int(0, dataP);
        return COAP_205_CONTENT;
#endif
    case LWM2M_SECURITY_SHORT_SERVER_ID:
        lwm2m_data_encode_int(targetP->shortID, dataP);
        return COAP_205_CONTENT;

    case LWM2M_SECURITY_HOLD_OFF_ID:
        lwm2m_data_encode_int(targetP->clientHoldOffTime, dataP);
        return COAP_205_CONTENT;

    default:
        return COAP_404_NOT_FOUND;
    }
}

static uint8_t prv_security_read(uint16_t instanceId,
                                 int * numDataP,
                                 lwm2m_data_t ** dataArrayP,
                                 lwm2m_object_t * objectP)
{
    security_instance_t * targetP;
    uint8_t result;
    int i;

    targetP = (security_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    // is the server asking for the full instance ?
    if (*numDataP == 0)
    {
        uint16_t resList[] = {LWM2M_SECURITY_URI_ID,
                              LWM2M_SECURITY_BOOTSTRAP_ID,
                              LWM2M_SECURITY_SECURITY_ID,
                              #ifdef LWM2M_WITH_DTLS
                              LWM2M_SECURITY_PUBLIC_KEY_ID,
                              LWM2M_SECURITY_SERVER_PUBLIC_KEY_ID,
                              LWM2M_SECURITY_SECRET_KEY_ID,
                              #endif
                              #ifdef LWM2M_WITH_SMS
                              LWM2M_SECURITY_SMS_SECURITY_ID,
                              LWM2M_SECURITY_SMS_KEY_PARAM_ID,
                              LWM2M_SECURITY_SMS_SECRET_KEY_ID,
                              LWM2M_SECURITY_SMS_SERVER_NUMBER_ID,
                              #endif
                              LWM2M_SECURITY_SHORT_SERVER_ID,
                              LWM2M_SECURITY_HOLD_OFF_ID};
        int nbRes = sizeof(resList)/sizeof(uint16_t);

        *dataArrayP = lwm2m_data_new(nbRes);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = nbRes;
        for (i = 0 ; i < nbRes ; i++)
        {
            (*dataArrayP)[i].id = resList[i];
        }
    }

    i = 0;
    do
    {
        result = prv_get_value((*dataArrayP) + i, targetP);
        i++;
    } while (i < *numDataP && result == COAP_205_CONTENT);

    return result;
}

#ifdef LWM2M_BOOTSTRAP

static uint8_t prv_security_write(uint16_t instanceId,
                                  int numData,
                                  lwm2m_data_t * dataArray,
                                  lwm2m_object_t * objectP)
{
    security_instance_t * targetP;
    int i;
    uint8_t result = COAP_204_CHANGED;

    targetP = (security_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP)
    {
        return COAP_404_NOT_FOUND;
    }

    i = 0;
    do {
        switch (dataArray[i].id)
        {
        case LWM2M_SECURITY_URI_ID:
            if (targetP->uri != NULL) lwm2m_free(targetP->uri);
            targetP->uri = (char *)lwm2m_malloc(dataArray[i].value.asBuffer.length + 1);
            memset(targetP->uri, 0, dataArray[i].value.asBuffer.length + 1);
            if (targetP->uri != NULL)
            {
                strncpy(targetP->uri, (char*)dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_500_INTERNAL_SERVER_ERROR;
            }
            break;

        case LWM2M_SECURITY_BOOTSTRAP_ID:
            if (1 == lwm2m_data_decode_bool(dataArray + i, &(targetP->isBootstrap)))
            {
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;

        case LWM2M_SECURITY_SECURITY_ID:
        {
            int64_t value;

            if (1 == lwm2m_data_decode_int(dataArray + i, &value))
            {
                if (value >= 0 && value <= 3)
                {
                    targetP->securityMode = value;
                    result = COAP_204_CHANGED;
                }
                else
                {
                    result = COAP_406_NOT_ACCEPTABLE;
                }
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
        }
        break;
        case LWM2M_SECURITY_PUBLIC_KEY_ID:
            if (targetP->publicIdentity != NULL) lwm2m_free(targetP->publicIdentity);
            targetP->publicIdentity = (char *)lwm2m_malloc(dataArray[i].value.asBuffer.length +1);
            memset(targetP->publicIdentity, 0, dataArray[i].value.asBuffer.length + 1);
            if (targetP->publicIdentity != NULL)
            {
                memcpy(targetP->publicIdentity, (char*)dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                targetP->publicIdLen = dataArray[i].value.asBuffer.length;
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_500_INTERNAL_SERVER_ERROR;
            }
            break;

        case LWM2M_SECURITY_SERVER_PUBLIC_KEY_ID:
            if (targetP->serverPublicKey != NULL) lwm2m_free(targetP->serverPublicKey);
            targetP->serverPublicKey = (char *)lwm2m_malloc(dataArray[i].value.asBuffer.length +1);
            memset(targetP->serverPublicKey, 0, dataArray[i].value.asBuffer.length + 1);
            if (targetP->serverPublicKey != NULL)
            {
                memcpy(targetP->serverPublicKey, (char*)dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                targetP->serverPublicKeyLen = dataArray[i].value.asBuffer.length;
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_500_INTERNAL_SERVER_ERROR;
            }
            break;

        case LWM2M_SECURITY_SECRET_KEY_ID:
            if (targetP->secretKey != NULL) lwm2m_free(targetP->secretKey);
            targetP->secretKey = (char *)lwm2m_malloc(dataArray[i].value.asBuffer.length +1);
            memset(targetP->secretKey, 0, dataArray[i].value.asBuffer.length + 1);
            if (targetP->secretKey != NULL)
            {
                memcpy(targetP->secretKey, (char*)dataArray[i].value.asBuffer.buffer, dataArray[i].value.asBuffer.length);
                targetP->secretKeyLen = dataArray[i].value.asBuffer.length;
                result = COAP_204_CHANGED;
            }
            else
            {
                result = COAP_500_INTERNAL_SERVER_ERROR;
            }
            break;

        case LWM2M_SECURITY_SMS_SECURITY_ID:
            // Let just ignore this
            result = COAP_204_CHANGED;
            break;

        case LWM2M_SECURITY_SMS_KEY_PARAM_ID:
            // Let just ignore this
            result = COAP_204_CHANGED;
            break;

        case LWM2M_SECURITY_SMS_SECRET_KEY_ID:
            // Let just ignore this
            result = COAP_204_CHANGED;
            break;

        case LWM2M_SECURITY_SMS_SERVER_NUMBER_ID:
            // Let just ignore this
            result = COAP_204_CHANGED;
            break;

        case LWM2M_SECURITY_SHORT_SERVER_ID:
        {
            int64_t value;

            if (1 == lwm2m_data_decode_int(dataArray + i, &value))
            {
                if (value >= 0 && value <= 0xFFFF)
                {
                    targetP->shortID = value;
                    result = COAP_204_CHANGED;
                }
                else
                {
                    result = COAP_406_NOT_ACCEPTABLE;
                }
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
        }
        break;

        case LWM2M_SECURITY_HOLD_OFF_ID:
        {
            int64_t value;

            if (1 == lwm2m_data_decode_int(dataArray + i, &value))
            {
                if (value >= 0 && value <= 0xFFFF)
                {
                    targetP->clientHoldOffTime = value;
                    result = COAP_204_CHANGED;
                }
                else
                {
                    result = COAP_406_NOT_ACCEPTABLE;
                }
            }
            else
            {
                result = COAP_400_BAD_REQUEST;
            }
            break;
        }
        default:
            return COAP_404_NOT_FOUND;
        }
        i++;
    } while (i < numData && result == COAP_204_CHANGED);

    return result;
}

static uint8_t prv_security_delete(uint16_t id,
                                   lwm2m_object_t * objectP)
{
    security_instance_t * targetP;

    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, id, (lwm2m_list_t **)&targetP);
    if (NULL == targetP) return COAP_404_NOT_FOUND;
    if (NULL != targetP->uri)
    {
        lwm2m_free(targetP->uri);
    }

    lwm2m_free(targetP);

    return COAP_202_DELETED;
}

static uint8_t prv_security_create(uint16_t instanceId,
                                   int numData,
                                   lwm2m_data_t * dataArray,
                                   lwm2m_object_t * objectP)
{
    security_instance_t * targetP;
    uint8_t result;

    targetP = (security_instance_t *)lwm2m_malloc(sizeof(security_instance_t));
    if (NULL == targetP) return COAP_500_INTERNAL_SERVER_ERROR;
    memset(targetP, 0, sizeof(security_instance_t));

    targetP->instanceId = instanceId;
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);

    result = prv_security_write(instanceId, numData, dataArray, objectP);

    if (result != COAP_204_CHANGED)
    {
        (void)prv_security_delete(instanceId, objectP);
    }
    else
    {
        result = COAP_201_CREATED;
    }

    return result;
}
#endif // LWM2M_BOOTSTRAP

#ifdef LWM2M_WITH_LOGS
void display_security_object(lwm2m_object_t * object)
{
    #ifdef DEBUG
        fprintf(stdout, "  /%u: Security object, instances:\r\n", object->objID);
    #endif
    security_instance_t * instance = (security_instance_t *)object->instanceList;
    while (instance != NULL)
    {
        #ifdef DEBUG
        fprintf(stdout, "    /%u/%u: instanceId: %u, uri: %s, isBootstrap: %s, shortId: %u, clientHoldOffTime: %u\r\n",
                object->objID, instance->instanceId,
                instance->instanceId, instance->uri, instance->isBootstrap ? "true" : "false",
                instance->shortID, instance->clientHoldOffTime);
        #endif
        instance = (security_instance_t *)instance->next;
    }
}
#endif

lwm2m_object_t * init_security_object()
{
    memset(&security_object, 0, sizeof(lwm2m_object_t));
    security_object.objID = 0;
    security_object.readFunc = prv_security_read;

    #ifdef LWM2M_BOOTSTRAP
    security_object.writeFunc = prv_security_write;
    security_object.createFunc = prv_security_create;
    security_object.deleteFunc = prv_security_delete;
    #endif

    return &security_object;
}


#include "logger.h"

#include <HardwareSerial.h>

void lwm2m_log(char *x)
{
#ifdef DEBUG
    Serial.println(x);
#endif
}
void lwm2m_log_int(int number)
{
#ifdef DEBUG
    Serial.println(number);
#endif
}
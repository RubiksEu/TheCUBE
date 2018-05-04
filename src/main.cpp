#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "DHT.h"

#include "wakaama_simple_client.h"
#include "wakaama_network.h"
#include "wakaama_object_utils.h"

#include "object_temperature.h"
#include "object_humidity.h"
#include "object_led.h"
#include "object_location.h"

//constant wifi timer value
#define WIFI_CHECK 60

// The pin to the battery LED
#define BATTERY_LED 13

// The pin to the connection LED
#define CONNECTION_LED_PIN 5

// The pin to the accesspoint LED
#define AP_LED_PIN 4

// The pin to the LED to turn on below the given threshold
#define THRESHOLD_LED_1_PIN 13

// The pin to the LED to turn on above the given threshold
#define THRESHOLD_LED_2_PIN 12

// The pin to the slide switch between WiFi AP and client mode
#define SLIDE_SWITCH_PIN 14

#define RGB_LED_PIN 15

#define BUZZER_PIN 3

#define DHTPIN 0

// The DHT type (DHT 22 ( = AM2302))
#define DHTTYPE DHT11

// The DHT definition
DHT dht(DHTPIN, DHTTYPE, 11);

//RGB LED initialization
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(4, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

char ssid[] = "WIFI-POINT"; //  wifi network SSID (name)
char pass[] = "WIFI-PASSWORD";     // wifi network password

uint32_t lwm2m_server_lifetime = 120;
bool bootstrap = false;

uint16_t lwm2m_server_id = 123;
const char *lwm2m_server_bind = "U";
const char *lwm2m_server_uri = "coap://8.8.8.8:5683"; // LwM2M server ip
const char *lwm2m_client_psk = NULL;
const char *lwm2m_client_id = NULL;

lwm2m_context_t *client_context;

uint32_t timer_reading = 0;
uint32_t timer_wifi_check = 0;
bool isConnected = false;

/**
   Initializes the LEDs
*/
void initializeLEDs()
{
  pinMode(CONNECTION_LED_PIN, OUTPUT);
  pinMode(THRESHOLD_LED_1_PIN, OUTPUT);
  pinMode(THRESHOLD_LED_2_PIN, OUTPUT);
  pinMode(AP_LED_PIN, OUTPUT);
  pinMode(SLIDE_SWITCH_PIN, INPUT);
}

/**
   Toggles the LED
*/
void toggleLED(int nPin, bool bEnable)
{
  // Write high or low to the LED
  digitalWrite(nPin, (bEnable ? HIGH : LOW));
}

/**
   Set Color and delay for the RGB LEDs
*/
void setColor(int led, int redValue, int greenValue, int blueValue, int delayValue)
{
  pixels.setPixelColor(led, pixels.Color(redValue, greenValue, blueValue));
  pixels.show();
  delay(delayValue);
}

/** 
 *  Make a noise with the buzzer
 */
void timing_udelay(unsigned short freq)
{

  unsigned short count;
  float a, b, c;

  a = 1.11;
  b = 2.21;
  c = 1.23;

  for (count = 1; count < freq; count++)
  {
    c = a * 0.15;
    b = c * 1.11;
    pinMode(BUZZER_PIN, OUTPUT);
  };
};

/**
   Turn on buzzer
*/
void buzz(unsigned short freq, unsigned char period)
{

  unsigned short count;

  for (count = 1; count < period; count++)
  {
    timing_udelay(freq);
    digitalWrite(BUZZER_PIN, HIGH);
    timing_udelay(freq);
    digitalWrite(BUZZER_PIN, LOW);
  };
};


/**
   Play nice buzz
*/
void buzzNice()
{
  buzz(500, 40);
  buzz(250, 60);
  buzz(125, 120);
}

/**
   Play bad buzz
*/
void buzzBad()
{
  buzz(500, 120);
  buzz(250, 60);
  buzz(1000, 30);
}

/**
 *  Blinks given LED
 */
void blinkLED(int nPin, int nTimes)
{
  // Disable and wait for 2 seconds
  toggleLED(nPin, false);
  delay(2000);

  // Here we go
  for (int i = 0; i < nTimes; i++)
  {
    toggleLED(nPin, true);
    delay(1000);

    toggleLED(nPin, false);
    delay(1000);
  }
}

/**
   Set RGB LED after wifi connection
*/
void startUp()
{
  //green - wifi connected
  setColor(0, 0, 255, 0, 10);
  setColor(1, 0, 255, 0, 10);

  buzzNice();
  delay(2000);

  setColor(0, 0, 0, 0, 10); //off
  setColor(1, 0, 0, 0, 10); //off

  toggleLED(AP_LED_PIN, true);
  isConnected = true;
}

/**
 *  Gets temperature from the Cube
 */
float getTemperature()
{
  float tmp = dht.readTemperature();
  char attempts = 0;

  while (isnan(tmp))
  {
    tmp = dht.readTemperature();
    attempts++;
    delay(1000);

#ifdef DEBUG
    Serial.print("force getting temperature in loop; max 5 tries current value: ");
    Serial.println(tmp);
#endif

    if (attempts > 5)
    {
      tmp = 0;
      break;
    }
  }

  // small dirty correct; practice shows DHT11 gives a 2 degree deviation
  tmp = tmp - 2;
  return tmp;
}

/**
 * Gets humidity from the Cube
 */
float getHumidity()
{
  float hum = dht.readHumidity();
  char attempts = 0;

  while (isnan(hum))
  {
    hum = dht.readHumidity();
    attempts++;
    delay(1000);

#ifdef DEBUG
    Serial.println("force getting humidiy in loop; max 5 tries");
    Serial.println(hum);
#endif

    if (attempts > 5)
    {
      hum = 0;
      break;
    }
  }

  return hum;
}

/**
   Update value in lwm2m observeList
*/
void handle_value_changed(lwm2m_context_t *lwm2mH,
                          lwm2m_uri_t *uri,
                          const char *value,
                          size_t valueLength)
{
  lwm2m_object_t *object = (lwm2m_object_t *)LWM2M_LIST_FIND(lwm2mH->objectList, uri->objectId);

  if (NULL != object)
  {
    if (object->writeFunc != NULL)
    {
      lwm2m_data_t *dataP;
      int result;

      dataP = lwm2m_data_new(1);
      if (dataP == NULL)
      {
#ifdef DEBUG
        Serial.println("Internal allocation failure !");
#endif
        return;
      }
      dataP->id = uri->resourceId;
      lwm2m_data_encode_nstring(value, valueLength, dataP);

      result = object->writeFunc(uri->instanceId, 1, dataP, object);

      if (COAP_204_CHANGED != result)
      {
#ifdef DEBUG
        Serial.println("Failed to change value!");
#endif
      }
      else
      {
#ifdef DEBUG
        Serial.println("value changed!");
#endif
        lwm2m_resource_value_changed(lwm2mH, uri);
      }
      lwm2m_data_free(1, dataP);
      return;
    }
    else
    {
#ifdef DEBUG
      Serial.println("write not supported for specified resource!");
#endif
    }
    return;
  }
  else
  {
#ifdef DEBUG
    Serial.println("Object not found !");
#endif
  }
}

/**
   Prepare data to update
*/
void update_sensor_value(lwm2m_context_t *context, const char *sensorUri, double sensorValue)
{
  lwm2m_uri_t uri;
  char value[15];
  int valueLength;
  if (lwm2m_stringToUri(sensorUri, strlen(sensorUri), &uri))
  {
    valueLength = sprintf(value, "%f", sensorValue);
#ifdef DEBUG
    Serial.println(sensorValue);
#endif
    handle_value_changed(context, &uri, value, valueLength);
  }
}

/**
  Setup wifi and lwm2m connect
*/
void setup()
{
  Serial.begin(9600);
#ifdef DEBUG
  Serial.println("-");
  Serial.println("Starting ...");
#endif

  // Initialize DHT
  dht.begin();
  pixels.begin();
  // Initialize the LEDs
  initializeLEDs();
  setColor(0, 0, 0, 255, 10);
  setColor(1, 0, 0, 255, 10);

  randomSeed(analogRead(0));
#ifdef DEBUG
  Serial.print("Connecting to ");
  Serial.println(ssid);
#endif
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
  }
#ifdef DEBUG
  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif
  startUp();

  //setup device data
  device_instance_t *device_data = lwm2m_device_data_get();
  device_data->manufacturer = "Ericsson";
  device_data->model_name = "Cube 2.0";
  device_data->device_type = "CUBE";
  device_data->firmware_ver = "1.0";
  device_data->serial_number = "140234-645235-12353";

  client_context = lwm2m_client_init("Cube"); // lwm2m client name

  if (client_context == 0)
  {
#ifdef DEBUG
    Serial.println("Failed to initialize wakaama");
#endif
    return;
  }
  //add object (sensors)
  lwm2m_add_object(client_context, init_location_object());
  lwm2m_add_object(client_context, init_temperature_object());
  lwm2m_add_object(client_context, init_humidity_object());
  lwm2m_add_object(client_context, init_led_object());

  if (!(lwm2m_add_server(lwm2m_server_id, lwm2m_server_uri, lwm2m_server_lifetime, false, lwm2m_client_id, lwm2m_client_psk, 0)))
  {
#ifdef DEBUG
    Serial.println("lwm2m_add_server() failed!");
#endif
    return;
  }

  // Initialize the BUILTIN_LED pin as an output
  pinMode(BUILTIN_LED, OUTPUT);

  // // Wait for network to connect

  // // Init lwm2m network
  uint8_t bound_sockets = lwm2m_network_init(client_context, NULL);
  if (bound_sockets == 0)
  {
#ifdef DEBUG
    Serial.println("Failed to open socket");
#endif
  }
}

/**
 Check lwm2m status; update sensors values
*/
void loop()
{
  time_t tv_sec;
#ifdef DEBUG
  Serial.println("Start Loop");
#endif

/*
* This function does two things:
*  - first it does the work needed by liblwm2m (eg. (re)sending some packets).
*  - Secondly it adjusts the timeout value (default 60s) depending on the state of the transaction
*    (eg. retransmission) and the time between the next operation
*/
  uint8_t result = lwm2m_step(client_context, &tv_sec);
#ifdef DEBUG
  if (result == COAP_503_SERVICE_UNAVAILABLE)
  {
    Serial.println("No server found so far");
  }
  else if (result != 0)
  {
    Serial.println("lwm2m_step() failed:");
    Serial.println(result);
  }
#endif
  lwm2m_network_native_sock(client_context, 0);
  lwm2m_network_process(client_context);

  //update sensors value
  if (timer_reading > 15)
  {
#ifdef DEBUG
    Serial.print("Temperature: ");
    Serial.println(getTemperature());
#endif
    update_sensor_value(client_context, "/3303/0/5700", getTemperature());

#ifdef DEBUG
    Serial.print("Humidity: ");
    Serial.println(getHumidity());
#endif
    update_sensor_value(client_context, "/3304/0/5700", getHumidity());
    timer_reading = 0;
  }
  timer_reading++;
  delay(300);

  //check wifi connection
  if (timer_wifi_check > WIFI_CHECK)
  {
#ifdef DEBUG
    Serial.println("\n\n\nWIFI CHECK\n\n\n");
    Serial.println(millis());
#endif
    if (WiFi.isConnected())
    {
      if (!isConnected)
      {
        buzzNice();
        isConnected = true;
        toggleLED(AP_LED_PIN, true);
      }
    }
    else
    {
      if (isConnected)
      {
        buzzBad();
        isConnected = false;
        toggleLED(AP_LED_PIN, false);
      }
    }
    timer_wifi_check = 0;
  }
  else
  {
    timer_wifi_check++;
  }
}
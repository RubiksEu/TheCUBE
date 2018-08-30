//--- INCLUDES -----------------------------
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Wire.h>
#include "Sodaq_HTS221.h"
#include <Sodaq_UBlox_GPS.h>
#include "Adafruit_NeoPixel.h"
#include <Adafruit_Sensor.h>
#include <LSM303.h>
#include <SPI.h>
#include <SD.h>

#include "Defines.h"
#include "cube_display.h"
#include "variables.h"

#include <Scheduler.h>

#define USE_GPS 1
#define USE_PIR 0
#define DEBUG
#define SEND_DATA_INTERVAL 10
//------------------------------------------
extern volatile int Encoder_position_menu;
extern volatile bool enter_menu;
extern volatile bool select_button;

struct menuItem act_menu[5] = {
  {"FAN", false},
  {"RGB", false},
  {"LED1", false},
  {"LED2", false},
  {"RELAY", false}
};



//------SOFTWARE RESET FUNCTION---------------------------
static inline bool nvmReady(void) {
        return NVMCTRL->INTFLAG.reg & NVMCTRL_INTFLAG_READY;
}

__attribute__ ((long_call, section (".ramfunc")))
static void banzai() {
	// Disable all interrupts
	__disable_irq();

	// Erase application
	while (!nvmReady())
		;
	NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;
	NVMCTRL->ADDR.reg  = (uintptr_t)&NVM_MEMORY[APP_START / 4];
	NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMD_ER | NVMCTRL_CTRLA_CMDEX_KEY;
	while (!nvmReady())
		;

	// Reset the device
	NVIC_SystemReset() ;

	while (true);
}
//------------------------------------------

//--- VARIABLES ----------------------------
int16_t counter;
volatile unsigned char ENC_Button_pressed = false;
volatile unsigned char Encoder_position_changed;
volatile int Encoder_position = MAIN_MENU;
int16_t temperature;
int16_t humidity;
char tmpString[255];
char tmpString1[255];
int16_t sensorValue;
//float f;
String s, AT_response;
uint8_t c;
int16_t i,k;
bool res;

char initString[512];
char hex_str[512];

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, RGB_LED_PIN, NEO_GRB);
LSM303 accel;



int MENU_pos = 0;
int aState;
int aLastState;
File myFile;

struct payloadData {
  float temperature;
  float humidity;
  float luminance;
  float sound;
  float x_acc;
  float y_acc;
  float z_acc;
  float battery;
};

struct payloadData p;

int connectedToNetwork = 0;
bool socketCreated = false;
int force_reset_counter = 0;

char operatorName[40];
char IMEI[32];
char IMSI[32];


//***************************
// 2018-06-10
// SARA R4 HEX mode configuration
//      INPUT:
//              0 - HEX mode disabled
//              1 - HEX mode enabled
//---------------------------------------------------------------------------------
unsigned char SaraR4_HEX_mode(unsigned char what) {

  char *index_ptr;
  char command_str[512];
  unsigned char c;
  uint16_t cnt;
  unsigned char response;

  response = 1;
  sprintf(command_str, "AT+UDCONF=1,%d\r\n", what);
  send_AT_command(command_str);

};

void rgbLED(bool on) {
  pixels.show();

  if (on) {
    setColor(0,250, 189, 186,0); //red

    pixels.show();
  }
  else {
    setColor(0,0,0,0,0); //off
    pixels.show();
  }
}

void actuateActuatores() {
  for (int x=0; x < sizeof(act_menu) / sizeof(act_menu[0]); x++) {
    switch (x) {
      case 0:
        digitalWrite(FAN, act_menu[x].selected);
        break;
      case 1:
        rgbLED(act_menu[x].selected);
        break;
      case 2:
        digitalWrite(LED_RED, !act_menu[x].selected);
        break;
      case 3:
        digitalWrite(LED_BLUE, !act_menu[x].selected);
        break;
      case 4:

        break;
    }
  }
}

void sendSocketData(struct payloadData *p) {
  char *index_ptr;
  unsigned char c, cH, cL;
  uint16_t cnt;
  char tmp2[3];
  unsigned char response;
  unsigned short cnt2;

  DEBUG_STREAM.println("===============================");
  DEBUG_STREAM.println("START SEND SOCK DATA");


  sprintf(initString,
          "{\"Device_ID\":\"%s\",\"temp\":%.2f,\"lum\":%.2f,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"bat\":%.2f,\"hum\":%.2f,\"db\":%.2f}",
       IMEI, p->temperature, p->luminance, p->x_acc, p->y_acc, p->z_acc, p->battery, p->humidity, p->sound);

  // sprintf(initString, "hallo");

  DEBUG_STREAM.println("initString:");
  DEBUG_STREAM.println(initString);

  response = 1;

  memset(hex_str, 0x00, sizeof(hex_str));
  //---- let's prepare HEX_string from string -----
  cnt2 = 0;
  for (cnt=0;cnt<strlen(initString);cnt++) {
    c = (char)initString[cnt];
    sprintf(tmp2, "%02X", c);
    hex_str[cnt2++] = tmp2[0];
    hex_str[cnt2++] = tmp2[1];
  };

  sprintf(initString, "AT+USOST=0,\"%s\",%d,%d,\"%s\"\r\n", SERVER_IP, SERVER_PORT, strlen(hex_str)/2, hex_str);

  DEBUG_STREAM.println("1");
  send_AT_command(initString);

  i = AT_response.indexOf("+USOST: ");

  s = "Sent: " + AT_response.substring(i+10, i+11);
  s.toCharArray(tmpString1, 30);
  DEBUG_STREAM.println(s);
};
//======================================================================================================================

//*************************************************************************************************
// 2018-07-27
// Sending data payload
//-------------------------------------------------------------------------------------------------
void sendSocketDataTcp(struct payloadData *p) {
  char *index_ptr;
  unsigned char c, cH, cL;
  uint16_t cnt;
  char tmp2[3];
  unsigned char response;
  unsigned short cnt2;

  DEBUG_STREAM.println("===============================");
  DEBUG_STREAM.println("START SEND SOCK DATA FOR TCP");

  send_AT_command("AT+USOCO=0,\"51.38.234.231\",7778\r\n");

  //
  // sprintf(initString,
  //         "{\"Device_ID\":\"%s\",\"temp\":%.2f,\"lum\":%.2f,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"bat\":%.2f,\"hum\":%.2f,\"db\":%.2f}",
  //      DEVICE_ID, p->temperature, p->luminance, p->x_acc, p->y_acc, p->z_acc, p->battery, p->humidity, p->sound);

  sprintf(initString,"test1234");

  DEBUG_STREAM.println("initString:");
  DEBUG_STREAM.println(initString);

  response = 1;

  memset(hex_str, 0x00, sizeof(hex_str));
  //---- let's prepare HEX_string from string -----
  cnt2 = 0;
  for (cnt=0;cnt<strlen(initString);cnt++) {
    c = (char)initString[cnt];
    sprintf(tmp2, "%02X", c);
    hex_str[cnt2++] = tmp2[0];
    hex_str[cnt2++] = tmp2[1];
  };

  sprintf(initString, "AT+USOWR=0,%d,\"%s\"\r\n", SERVER_IP, SERVER_PORT, strlen(hex_str)/2, hex_str);

  DEBUG_STREAM.println("1");
  send_AT_command(initString);

  i = AT_response.indexOf("+USOWR: ");

  s = "Sent: " + AT_response.substring(i+10, i+11);
  s.toCharArray(tmpString1, 30);
  DEBUG_STREAM.println(s);
};
//======================================================================================================================




void createSocket() {
  DEBUG_STREAM.println("===============================");
  DEBUG_STREAM.println("START CREATE SOCKET");
  Encoder_position_changed = 0;
  res = send_AT_command("AT+USOCR=17,7778\r\n");
  // res = send_AT_command("AT+USOCR=6,10001\r\n");
  delay(1500);
  DEBUG_STREAM.println("===============================");
  DEBUG_STREAM.println("END CREATE SOCKET");
}

int getNetworkSignal() {
  DEBUG_STREAM.println("===============================");
  DEBUG_STREAM.println("START GET CSQ");
  send_AT_command("AT+CSQ\r\n");
  delay(200);
  send_AT_command("AT+CSQ\r\n");

  int nrOfAttempts = 20;

  DEBUG_STREAM.print("FULL Response:");
  DEBUG_STREAM.println(AT_response);

  while (AT_response.indexOf("+CSQ: ") == -1 ){
    delay(300);
    send_AT_command("AT+CSQ\r\n");

    DEBUG_STREAM.print("FULL Response:");
    DEBUG_STREAM.println(AT_response);

    nrOfAttempts--;

    if (nrOfAttempts == 0) {
      return -1;
    }
  }

  int commaPosition = AT_response.indexOf(",");


  int csq = atoi( AT_response.substring(commaPosition-2, commaPosition).c_str());

  DEBUG_STREAM.print("SUBTRING Response:");
  DEBUG_STREAM.println( AT_response.substring(commaPosition-2, commaPosition));

  DEBUG_STREAM.print("csq: ");
  DEBUG_STREAM.println(csq);

  DEBUG_STREAM.println("END GET CSQ");
  DEBUG_STREAM.println("===============================");
  return csq;
}


int getSocketData() {
  DEBUG_STREAM.println("===============================");
  DEBUG_STREAM.println("START getSocketData");

  SaraR4_HEX_mode(0);
  delay(250);

  send_AT_command("AT+USORF=0,512\r\n");
  //send_AT_command("AT");

  for (int x=0 ; AT_response.indexOf("+USORF: ") == -1; x++ ) {
    DEBUG_STREAM.println(AT_response);
    if (x > 3) {
      return -1;
    }
    send_AT_command("AT");
    delay(500);
  }

  char * pch;
  pch = strrchr(AT_response.c_str(),',');

  DEBUG_STREAM.print("PCH: ");
  DEBUG_STREAM.println(pch);

  // char ascii[32] = "";
  // int c;
  // char tmp[3];
  //
  // for (;pch[0] && pch[1] && sscanf(pch, "%2x", &c); pch += 2)
  // {
  //   sprintf(tmp, "%c", c);
  //   strcat (ascii, tmp);
  // }



  DEBUG_STREAM.print("UDP SERVER RESPONSE: ");
  DEBUG_STREAM.println(pch);

  // DEBUG_STREAM.print("TO ASCII: ");
  // DEBUG_STREAM.println(ascii);

  DEBUG_STREAM.println("END GET getSocketData");
  DEBUG_STREAM.println("===============================");

  SaraR4_HEX_mode(1);

  delay(250);
  return 1;
}

int getNetworkAttachment() {
  DEBUG_STREAM.println("===============================");
  DEBUG_STREAM.println("START GET CGATT");

  int nrOfAttempts = 20;

  send_AT_command("AT+CGATT?\r\n");
  delay(200);
  send_AT_command("AT+CGATT?\r\n");

  DEBUG_STREAM.print("FULL Response:");
  DEBUG_STREAM.println(AT_response);

  while (AT_response.indexOf("+CGATT") == -1 || AT_response.indexOf("1OK") == -1){
    DEBUG_STREAM.print("FULL Response:");
    DEBUG_STREAM.println(AT_response);
    send_AT_command("AT+CGATT?\r\n");
    nrOfAttempts--;

    if (nrOfAttempts == 0) {
      DEBUG_STREAM.println("END GET CGATT NO CONNECTION");
      DEBUG_STREAM.println("===============================");
      return 0;
    }

    delay(1500);
  }

  DEBUG_STREAM.println("END GET CGATT SUCCESFULL");
  DEBUG_STREAM.println("===============================");

  return 1;


}

void getIMEI() {
  DEBUG_STREAM.println("===============================");
  DEBUG_STREAM.println("GET IMEI (CGSN)");
  send_AT_command("AT+CGSN\r\n");
  int index = 0;
  //send_AT_command("AT");

  for (int x=0 ; (index = AT_response.indexOf("+CGSN")) == -1; x++ ) {
    DEBUG_STREAM.println(AT_response);
    if (x > 3) {
      strcpy(IMEI, "IMEI NA");
      return;
    }
    send_AT_command("AT");
    delay(500);
  }

  strcpy(IMEI, AT_response.substring(index+strlen("+IMEI"), AT_response.length()-2).c_str());

  DEBUG_STREAM.print("IMEI ");
  DEBUG_STREAM.println(IMEI);
}

void getIMSI() {
  DEBUG_STREAM.println("===============================");
  DEBUG_STREAM.println("GET IMSI (CIMI)");
  send_AT_command("AT+CIMI\r\n");

  int index = 0;


  for (int x=0 ; (index = AT_response.indexOf("+CIMI")) == -1; x++ ) {
    DEBUG_STREAM.println(AT_response);
    if (x > 3) {
      strcpy(IMEI, "IMSI NA");
      return;
    }
    send_AT_command("AT");
    delay(500);
  }

  strcpy(IMSI, AT_response.substring(index+strlen("+CIMI"), AT_response.length()-2).c_str());

  DEBUG_STREAM.print("IMSI ");
  DEBUG_STREAM.println(IMSI);
}


void getOperator() {
  int lastComma, secondLastComma;

  DEBUG_STREAM.println("===============================");
  DEBUG_STREAM.println("GET OPERATOR (COPS)");
  send_AT_command("AT+COPS?\r\n");
  send_AT_command("AT+COPS?\r\n");
  //send_AT_command("AT");

  for (int x=0 ; AT_response.indexOf("COPS") == -1; x++ ) {
    DEBUG_STREAM.println(AT_response);
    if (x > 3) {
      strcpy(operatorName, "Operator Name NA");
      return;
    }
    send_AT_command("AT");
    delay(500);
  }

  lastComma = AT_response.lastIndexOf('","');
  secondLastComma = AT_response.lastIndexOf(',', lastComma - 1 );

  strcpy(operatorName, AT_response.substring(secondLastComma+2, lastComma).c_str());

  DEBUG_STREAM.print("OPERATOR ");
  DEBUG_STREAM.println(operatorName);

}

void modemInit() {
  //--- Enable VCC1 -------------------------------------------
  digitalWrite(VCC1_ENABLE, HIGH); // VCC1 -> ON

  //--- Enable N2 power ---------------------------------------
  DEBUG_STREAM.println("uBLOX init...");
  digitalWrite(N2_ENABLE, HIGH);
  delay(2000);
  DEBUG_STREAM.println("[Setup] N2 enabled");

  //--- Turn the power on -------------------------------------
  digitalWrite(POWER_ON_PIN, LOW);
  delay(200);
  digitalWrite(POWER_ON_PIN, HIGH);

  updateSplashScreen("30%%");


  //--- Init UBLOX NB-IoT module ------------------------------
  //delay(3000);
  MODEM_STREAM.begin(115200);
  delay(2000);
//  res = send_AT_command("AT+CFUN=15\r\n");
  DEBUG_STREAM.println(tmpString);
  delay(5000);
  updateSplashScreen("40%%");
  res = send_AT_command("AT");
  DEBUG_STREAM.println(tmpString);
  //res = send_AT_command("AT+URAT=8,7\r\n");
  //DEBUG_STREAM.println(tmpString);
  delay(500);
  updateSplashScreen("50%%");
  res = send_AT_command("AT+CEREG=3\r\n");
 DEBUG_STREAM.println(tmpString);
  delay(500);
  updateSplashScreen("60%%");
  res = send_AT_command("AT+CMEE=2\r\n");
  DEBUG_STREAM.println(tmpString);
  delay(500);
  res = send_AT_command("AT+CFUN=1\r\n");
  DEBUG_STREAM.println(tmpString);
  delay(500);
  //res = send_AT_command("AT+COPS=1,2,\"20408\"\r\n");
  res = send_AT_command("AT+COPS=0,0\r\n");
  DEBUG_STREAM.println(tmpString);
//  delay(1500);
  updateSplashScreen("70%%");
  res = send_AT_command("AT");
  DEBUG_STREAM.println(tmpString);
  delay(500);
//  res = send_AT_command("AT+CGDCONT=1, \"IP\",\"simpoint.m2m\"\r\n");
  DEBUG_STREAM.println(tmpString);
//  delay(1500);
  updateSplashScreen("80%%");
  res = send_AT_command("AT+CGDCONT?\r\n");
  DEBUG_STREAM.println(tmpString);
//  delay(1500);
  updateSplashScreen("90%%");
  SaraR4_HEX_mode(1);
  delay(1500);

  getIMEI();
  delay(500);
  getIMSI();
  delay(500);
  getOperator();
  delay(500);
}




//***************************************************************************
// 2018-06-24
// Sending AT command
//---------------------------------------------------------------------------
bool send_AT_command(const char* command_string) {

  int timeout, cnt;
  unsigned char response;

    timeout = 500;
    response = 0;
    AT_response = "";
    for (cnt=0; cnt< sizeof(tmpString); cnt++) {
      tmpString[cnt] = 0x00;
    };
    cnt = 0;
    // DEBUG_STREAM.println("*********************************");
    // DEBUG_STREAM.println(command_string);
    // DEBUG_STREAM.println("--------------");
    MODEM_STREAM.println(command_string);
    DEBUG_STREAM.println("command sent.");
    while((timeout>0) && (!response)) {
      // DEBUG_STREAM.print("timeout1 ");
      // DEBUG_STREAM.println(timeout);
      // DEBUG_STREAM.print("response1 ");
      // DEBUG_STREAM.println(response);

      while(MODEM_STREAM.available()){
        // DEBUG_STREAM.print("modemstream available");
        // DEBUG_STREAM.println(MODEM_STREAM.available());
        c = MODEM_STREAM.read();
        DEBUG_STREAM.write(c);
        if ((c != '\r') && (c != '\n')) {
          tmpString[cnt++] = c;
          AT_response+= char(c);
        };
        response = 1;
  //      yield();
      };
      timeout--;
  //    yield();
      delay(10);

    };

    return 1;
}
//===========================================================================



//***************************************************************************
// 2018-07-01
// Find a GPS fix, but first wait a while
//
//--------------------------------------------------------------------------
void find_fix(uint32_t delay_until)
{
    DEBUG_STREAM.println(String("delay ... ") + delay_until + String("ms"));
    delay(delay_until);

    uint32_t start = millis();
    uint32_t timeout = 300L * 1000;
    DEBUG_STREAM.println(String("waiting for fix ..., timeout=") + timeout + String("ms"));
    if (sodaq_gps.scan(false, timeout)) {
        DEBUG_STREAM.println(String(" time to find fix: ") + (millis() - start) + String("ms"));
        DEBUG_STREAM.println(String(" datetime = ") + sodaq_gps.getDateTimeString());
        DEBUG_STREAM.println(String(" lat = ") + String(sodaq_gps.getLat(), 7));
        DEBUG_STREAM.println(String(" lon = ") + String(sodaq_gps.getLon(), 7));
        DEBUG_STREAM.println(String(" num sats = ") + String(sodaq_gps.getNumberOfSatellites()));

    } else {
        DEBUG_STREAM.println("No Fix");
    };
};
//===========================================================================

//***************************************************************************
//simple function which takes values for the red, green and blue led and also
//a delay
//---------------------------------------------------------------------------
void setColor(int led, int redValue, int greenValue, int blueValue, int delayValue)
{
  DEBUG_STREAM.print("setColor " );
  DEBUG_STREAM.print("R " );
  DEBUG_STREAM.print(redValue);
  DEBUG_STREAM.print(" G " );
  DEBUG_STREAM.print(greenValue);
  DEBUG_STREAM.print(" B " );
  DEBUG_STREAM.println(blueValue);
  pixels.setPixelColor(led, pixels.Color(redValue, greenValue, blueValue));
  pixels.show();
  delay(delayValue);
}
//===========================================================================


//*************************************************************************
// 2016-11-02
// Description:
//
//-------------------------------------------------------------------------
void timing_udelay(unsigned short freq) {
  unsigned short count;
  float a, b, c;

  a = 1.11;
  b = 2.21;
  c = 1.23;
  for (count=1; count<freq; count++) {
    c = a * 0.15;
    b = c * 1.11;
    pinMode(BUZZER, OUTPUT);
  };
};
//=========================================================================


//*************************************************************************
// 2016-11-02
// Description:
//
//-------------------------------------------------------------------------
void make_sound(unsigned short freq, unsigned char period) {

  unsigned short count;

  for (count=1; count<period; count++) {
      timing_udelay(freq);
      digitalWrite(BUZZER, HIGH);
      timing_udelay(freq);
      digitalWrite(BUZZER, LOW);
  };
};
//=========================================================================

//****************************************************************************
// 2018-06-23
// Encoder interrupt
//----------------------------------------------------------------------------
void Encoder_interrupt(){

  SerialUSB.print("enter_menu: ");
  SerialUSB.println(enter_menu);
  SerialUSB.print("true: ");
  SerialUSB.println(true);
  SerialUSB.print("false: ");
  SerialUSB.println(false);

  if (enter_menu && Encoder_position == ACT_MENU) {
    aState = digitalRead(ENC_A); // Reads the "current" state of the outputA
    // If the previous and the current state of the outputA are different,
    // that means a Pulse has occured
    if (aState != aLastState){
      // If the outputB state is different to the outputA state,
      // that means the encoder is rotating clockwise
      if (digitalRead(ENC_B) != aState) {
        Encoder_position_menu++;
        make_sound(10,  120); // nice
        if (Encoder_position == ACT_MENU && Encoder_position_menu >= (sizeof(act_menu) / sizeof(act_menu[0]))) {
          Encoder_position_menu = 0;
        }
      }
      else {
        if(Encoder_position_menu > 0) {
           Encoder_position_menu--;
           make_sound(10,  120); // nice
         };
      }
      SerialUSB.print("Encoder_position_menu: ");
      SerialUSB.println(Encoder_position_menu);
    }
    aLastState = aState; // Updates the previous state of the outputA with the current state
  }
  else {
   aState = digitalRead(ENC_A); // Reads the "current" state of the outputA
   // If the previous and the current state of the outputA are different,
   // that means a Pulse has occured
   if (aState != aLastState){
     // If the outputB state is different to the outputA state,
     // that means the encoder is rotating clockwise
     if (digitalRead(ENC_B) != aState) {
       Encoder_position++;
       make_sound(10,  120); // nice
       if(Encoder_position >= NR_OF_MENU_OPTIONS) {
        Encoder_position = MAIN_MENU;
       };
       Encoder_position_changed = true;
     } else {
       if(Encoder_position > MAIN_MENU) {
          make_sound(10,  120); // nice
          Encoder_position--;
          Encoder_position_changed = true;
        };
     }
     SerialUSB.print("Encoder Position: ");
     SerialUSB.println(Encoder_position);
   }
   aLastState = aState; // Updates the previous state of the outputA with the current state
       //----------------------------------------------------------
  }
};
//============================================================================



//****************************************************************************
// 2018-06-23
// Encoder button interrupt
//----------------------------------------------------------------------------
void Encoder_button_interrupt(){
  int cnt, cnt1, a;

  for(cnt=0;cnt<500;cnt++){
    for(cnt1=0;cnt1<1000;cnt1++){
      a = digitalRead(ENC_BUTTON);
    };
  };

  // ENC_Button_pressed = true;
  DEBUG_STREAM.print ("encoder button interrupt ");
  enter_menu = !enter_menu;

};
//============================================================================

void select_button_pressed() {
  DEBUG_STREAM.print ("select_button_pressed");
  select_button = true;
}

//********************************************************************************************************
// 2017-11-30
// SETUP
//--------------------------------------------------------------------------------------------------------
void setup() {

  delay(2000); // time required for USB port to reconfigure after upload

  //--- Serial definiions -------------------------------------
  DEBUG_STREAM.begin(115200);
  MODEM_STREAM.begin(115200);

  //--- GPIO init ---------------------------------------------
  pinMode(VCC1_ENABLE, OUTPUT);
  pinMode(VCC2_ENABLE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(N2_ENABLE, OUTPUT);
  pinMode(RGB_LED_PIN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(LIGHT_VALUE_PIN, INPUT);
  pinMode(SOUND_VALUE_PIN, INPUT);
  pinMode(WAKE_UP_PIN, INPUT);
  pinMode(BATT_VOLTAGE_PIN, INPUT);
  pinMode(BATT_VOLTAGE_ENABLE, OUTPUT);
  pinMode(RELAY_IN1, OUTPUT);
  pinMode(RELAY_IN2, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(BATT_CHARGING, INPUT_PULLUP);
  pinMode(ENC_BUTTON, INPUT);
  pinMode(POWER_ON_PIN, OUTPUT);

  digitalWrite(POWER_ON_PIN, HIGH);
  digitalWrite(LED_BLUE, HIGH);            // LED_BLUE -> OFF
  digitalWrite(LED_RED, HIGH);             // LED_RED -> OFF
  digitalWrite(N2_ENABLE, LOW);            // VCC1 -> OFF
  digitalWrite(VCC1_ENABLE, LOW);          // VCC1 -> OFF
  digitalWrite(VCC2_ENABLE, LOW);          // VCC2 -> OFF
  digitalWrite(BUZZER, LOW);               // BUZZER -> OFF
  digitalWrite(BATT_VOLTAGE_ENABLE, LOW);  // BATT_ENABLE -> OFF
  digitalWrite(RELAY_IN1, LOW);            // RELAY_IN1 -> OFF
  digitalWrite(RELAY_IN2, LOW);            // RELAY_IN2 -> OFF
  digitalWrite(FAN, LOW);                  // FAN -> OFF
  digitalWrite(PIR_PIN, LOW);              // PIR pullDown

  attachInterrupt(digitalPinToInterrupt(ENC_A), Encoder_interrupt, CHANGE  );
  attachInterrupt(digitalPinToInterrupt(ENC_BUTTON), Encoder_button_interrupt, FALLING  );
  attachInterrupt(digitalPinToInterrupt(WAKE_UP_PIN), select_button_pressed, FALLING  );

  ENC_Button_pressed = false;
  enter_menu = false;
  Encoder_position_changed = true;
  Encoder_position = MAIN_MENU;
  aLastState = digitalRead(ENC_A);


  // MODEM SERIAL CONNECTION
  MODEM_STREAM.begin(115200);
  delay(2000); // time required for USB port to reconfigure after upload

  //--- Wakeing up Cube4.1 resources --------------------------

  //--- Enable VCC1 -------------------------------------------
  digitalWrite(VCC1_ENABLE, HIGH); // VCC1 -> ON

  //--- Enable VCC2 -------------------------------------------
  digitalWrite(VCC2_ENABLE, HIGH); // VCC2 -> ON
  delay(1000);  // Important !!!
  //-----------------------------------------------------------

  //--- I2C init ----------------------------------------------
  Wire.begin();
  Wire.setClock(100000); // 100 kHz

  //--- OLED init ---------------------------------------------
  DEBUG_STREAM.println("[Setup] Trying to init OLED...");

  initDisplay();

  //--- Clear display ----------------------------
  clearDisplay();

  initSplashScreen();

  //---- Start with Splash --------------------//
  make_sound(500, 40); // nice
  updateSplashScreen("10%%");

  //--- HTS221 temp & hum sensor init --------------------------
  DEBUG_STREAM.println("[Setup] Trying to init HTS221...");
  for (int x = 0; x<5; x++) {
    if (hts221.begin() == true) {
      DEBUG_STREAM.println("[Setup] HTS221 initialized");
      break;
    }
    if (hts221.begin() == false) {
      DEBUG_STREAM.println("[Setup] Error while retrieving WHO_AM_I byte...");
      DEBUG_STREAM.print("[Setup] Attempt: ");
      DEBUG_STREAM.println(x);
      delay(250);
    };
  }


  updateSplashScreen("20%%");

  accel.init();
  accel.enableDefault();
  //-----------------------------------------------------------

  modemInit();

  pixels.begin();
  setColor(0,255,0,0,500); //red
  pixels.show();

  // MAIN LOOP TO BE USED FOR DISPLAY ADDITONAL LOOP FOR CONNECTIVITY
  Scheduler.startLoop(connectivityLoop);

  updateSplashScreen("100%%");

  // START 8s watchdog timer
   // sodaq_wdt_enable(WDT_PERIOD_8X);
};
//=== SETUP ============================================================================================


void mainMenu() {

  digitalWrite(BATT_VOLTAGE_ENABLE, HIGH);  // BATT_ENABLE -> ON
  digitalWrite(VCC2_ENABLE, HIGH); // VCC2 -> ON

  char temperature[64];
  char humidity[64];
  char luminance[64];
  char battery[64];
  char sound[64];

  float bat = analogRead(BATT_VOLTAGE_PIN);
  bat =  (3.3 / 1024) * bat * 2 * 1.000;

  sprintf(temperature, "%0.2f%cC", float(hts221.readTemperature() * 100)/100, 248); //248 == ASCI degree symbol
  sprintf(humidity, "%0.2f%%", float (hts221.readHumidity()));
  sprintf(luminance, "%d lux", analogRead(LIGHT_VALUE_PIN));
  sprintf(battery, "%0.2f V", bat);
  sprintf(sound, "%d dB", analogRead(SOUND_VALUE_PIN));

  // DEBUG_STREAM.println(temperature);
  // DEBUG_STREAM.println(humidity);
  // DEBUG_STREAM.println(luminance);
  // DEBUG_STREAM.println(battery);
  // DEBUG_STREAM.println(sound);

  if (Encoder_position_changed) {
    menuDisplay("1", "MAIN");

    Encoder_position_changed = false;
  }

  mainMenuDisplay(temperature, humidity, luminance, battery, sound);
}

void accMenu() {

  char xacc[64];
  char yacc[64];
  char zacc[64];

  accel.read();
  sprintf(xacc, "%+05d", accel.a.x/100);
  sprintf(yacc, "%+05d", accel.a.y/100);
  sprintf(zacc, "%+05d", accel.a.z/100);

  DEBUG_STREAM.print("xacc");
  DEBUG_STREAM.println(xacc);
  DEBUG_STREAM.print("yacc");
  DEBUG_STREAM.println(yacc);
  DEBUG_STREAM.print("zacc");
  DEBUG_STREAM.println(zacc);

  if (Encoder_position_changed) {
    menuDisplay("2", "AXIS");

    Encoder_position_changed = false;
  }

  accMenuDisplay(xacc, yacc, zacc);
}

void actMenu() {
  DEBUG_STREAM.println("ACT MENU");

  if (Encoder_position_changed) {
  //  DEBUG_STREAM.println("Act MENU display prepare");
    menuDisplay("4", "ACT");
    enter_menu = false;

    Encoder_position_changed = false;
  }
  actMenuDisplay(act_menu, &actuateActuatores);
}

void netMenu() {
  //DEBUG_STREAM.println("NET MENU");

  //char* IMEI = "004999010640000";
  //char* IMSI = "222107701772423"; // AT+CMCI
  //char* Operator = "Vodafone"; // AT+COPS?

  if (Encoder_position_changed) {
    DEBUG_STREAM.println("Net MENU display prepare");
    menuDisplay("3", "NET");
    enter_menu = false;

    Encoder_position_changed = false;
  }

  networkMenuDisplay(IMEI, IMSI, operatorName);
}

void readAllSensorValues(struct payloadData *p) {
  float temperature = float(hts221.readTemperature() * 100)/100;
  float humidity = float(hts221.readHumidity());
  float luminance = sensorValue = analogRead(LIGHT_VALUE_PIN);

  digitalWrite(VCC2_ENABLE, HIGH); // VCC2 -> ON
  float sound = analogRead(SOUND_VALUE_PIN);
  accel.read();
  float x_acc = accel.a.x;
  float y_acc = accel.a.y;
  float z_acc = accel.a.z;

  digitalWrite(BATT_VOLTAGE_ENABLE, HIGH);  // BATT_ENABLE -> ON
  float battery = analogRead(BATT_VOLTAGE_PIN);
  battery =  (3.3 / 1024) * battery * 2 * 1.000;

  p->battery = battery;
  p->temperature = temperature;
  p->humidity = humidity;
  p->luminance = luminance;
  p->sound = sound;
  p->x_acc = x_acc;
  p->y_acc = y_acc;
  p->z_acc = z_acc;
}


//*******************************************************************************************************
// M A I N   L O O P
//-------------------------------------------------------------------------------------------------------
void loop() {

  while(1) {

    switch (Encoder_position) {
      case MAIN_MENU:
        //DEBUG_STREAM.println("Menu 1");
        mainMenu();
        break;
      case ACC_MENU:
        //DEBUG_STREAM.println("Menu 2");
        accMenu();
        break;
      case NET_MENU:
        //DEBUG_STREAM.println("Menu 3");
        netMenu();
        break;
      case ACT_MENU:
        //DEBUG_STREAM.println("Menu 3");
        actMenu();
        break;
      default:
        DEBUG_STREAM.println("No menu for encoder position");
    }

    delay(MAIN_LOOP_INTERVAL);
    yield();
  }
}



void connectivityLoop() {
  int signalStrength = 99;

  DEBUG_STREAM.print("connectivityLoop started");

  DEBUG_STREAM.print("---Getting Signal Stength---");
  signalStrength = getNetworkSignal();
  DEBUG_STREAM.print("Signal Stength: ");
  DEBUG_STREAM.println(signalStrength);

  if (signalStrength < 99) {
    DEBUG_STREAM.print("---Getting Network ATTACHEMENT---");
    connectedToNetwork = getNetworkAttachment();

    DEBUG_STREAM.print("Network Attachement: ");
    DEBUG_STREAM.println(connectedToNetwork);

    // CHECK FOR NETWORK ATTACHEMENT AND SIGNAL
    if (connectedToNetwork == 1) {
        pixels.show();
        setColor(0,0,0,0,0); //off
        setColor(0,0,255,0,0); //green
        pixels.show();

        if (!socketCreated) {
          createSocket();
          socketCreated = true; // TODO Check if needed only once
        }

        DEBUG_STREAM.println("GOING TO SEND DATA");
        readAllSensorValues(&p);

        digitalWrite(LED_BLUE, LOW);
        sendSocketData(&p);
        // sendSocketDataTcp(&p);
        digitalWrite(LED_BLUE, HIGH);
        delay(500);

        getSocketData();

    }
    else {
        pixels.show();
        setColor(0,0,0,0,0); //off
        setColor(0,244, 0, 0,0); //red
        pixels.show();
        force_reset_counter++;
        DEBUG_STREAM.print("force_reset_counter");
        DEBUG_STREAM.println(force_reset_counter);
    }
  }
  else {
    pixels.show();
    setColor(0,0,0,0,0); //off
    setColor(0,244, 0, 0,0); //red

    pixels.show();
  }

  // When SOFT_RESET_COUNTER is reached, the device will be forced to reset.
  if (force_reset_counter == SOFT_RESET_COUNTER) {
    banzai();
  }

  delay(SEND_DATA_INTERVAL);

}

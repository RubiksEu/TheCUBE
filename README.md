The Cube 2.0 (WiFi)
==================================

Installation
--------------

1. Install Visual Studio Code (or ATOM).

2. In Extensions, make sure that you have installed: 
    - C/C++;
    - PlatformIO IDE.
3. After installation of required extensions, open Platfomio Home Page (it should open automatically after restart or press Ctrl+Shift+P and type "PlatformIO: Home")

4. On Home page, in the left side choose "Libraries" menu. Setup following libraries:
    - Adafruit ESP8266 by Adafruit (^v1.0.0);
    - Adafruit NeoPixel by Adafruit (^v1.1.5).
5. The platform for Cube programming (ESP8266) should install automatically after the first Build. If not, choose "Platforms" menu on PlatformIO Home page and install "Espressif 8266".

6. Code is ready to build. You can build it just clicking on the button "Build" (tick) on the blue panel at the bottom or using console (Ctrl+Shift+P) and putting "PlatformIO: Build".
After some time, in console, you should see this message.

        Calculating size .pioenvs\d1_mini\firmware.elf
        Building .pioenvs\d1_mini\firmware.bin
        text       data     bss     dec     hex filename
        303431     7348   30512  341291   5352b .pioenvs\d1_mini\firmware.elf
        ========================================================================================== [SUCCESS] Took 8.99 seconds ==========================================================================================
        
        Terminal will be reused by tasks, press any key to close it.

7. To flash code, you should use USB to TTL serial adapter (like CP2102) and plug it in the Cube as shown in figure (`adapter.png`).

![alt text](./adapter.png)

Click on the button "Upload" (right-arrow) or using console type "PlatformIO: Upload". PlatfomIO itself will find required COM port and flash the code.

8. For watching logs - you should add flag "-DDEBUG" in `platfomio.ini`, flash it and open Serial Monitor - button "Serial Monitor" (cable) at the bottom.

Setup
--------------

1. Replace with your SSID and WiFi password.

        58    
        59      char ssid[] = "WIFI-POINT"; //  wifi network SSID (name)
        60      char pass[] = "WIFI-PASSWORD"; // wifi network password
        61
2. Enter NoSec IP of LwM2M server

        66      
        67      const char *lwm2m_server_uri = "coap://8.8.8.8:5683"; // LwM2M server ip
        68      

3. Change the name of client:

        395     
        396     client_context = lwm2m_client_init("Cube"); // lwm2m client name
        397
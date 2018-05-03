#The Cube

Installation
--------------

1. Install Visual Studio Code (or ATOM).
2. In Extensions, make sure that you have installed: 
    - C/C++;
    - PlatformIO IDE.
3. After installation of required extensions, open Platfomio Home Page (it should open automatically after restart or press Ctrl+Shift+P and type "PlatformIO: Home")
4. On Home page, in the left side choose "Libraries" menu. Setup following libraries:
    - Adafruit GFX Library by Adafruit (^v1.2.3);
    - Adafruit NeoPixel by Adafruit (^v1.1.5);
    - Adafruit SSD1306 by Adafruit (^v1.1.2);
    - Sodaq_wdt by GabrielNotman (^v1.0.1);
    - ArduinoJson by Benoit Blanchon (^v5.13.0);
    - ArduinoLog by Thijs Elenbaas (^v1.0.2);
    - SD by Arduino (^v1.2.2).
5. The platform for Cube programming (Arduino M0) should install automatically after the first Build. If not, choose "Platforms" menu on PlatformIO Home page and install "Atmel SAM".

6. Before building replace `variant.h` file in `/.platformio` folder (located for instance in `C:\Users\user\.platformio\packages\framework-arduinosam\variants\arduino_mzero`). Replacement file is present next to README.md file.

7. Code is ready to build. You can build it just clicking on the button "Build" (tick) on the blue panel at the bottom or using console (Ctrl+Shift+P) and putting "PlatformIO: Build".
After some time, in console, you should see this message.

        99848      2248    3436  105532   19c3c .pioenvs\mzeroUSB\firmware.elf
        ================================================================================================ [SUCCESS] Took 23.51 seconds ================================================================================================

        Terminal will be reused by tasks, press any key to close it.

8. To flash code into the Cube, you should plug in the Cube, click on the button "Upload" (right-arrow) or using console type "PlatformIO: Upload". PlatfomIO itself will find required COM port and flash the code.

!WARNING: Do nothing when PlatformIO flashing code, even touch USB cable - you can brick the Cube.

9. For watching logs - you should add flag "-DDEBUG_LOG" in `platfomio.ini`, flash it and open Serial Monitor - button "Serial Monitor" (cable) at the bottom.

Setup
------------

To set up connection to your server you need to insert SD card with files `config.txt` and `conn.txt` (files are present next to README.md file) into the Cube.

`conn.txt`: has settings for connection

    {
        "id":"Cube", - name of your LwM2M client
        "address": "8.8.8.8", -  LwM2M server IP
        "port": 5683, - NoSec port of LwM2M server
        "latitude":52.3638418, - latitude for Location object
        "longitude":4.8942684 - longitude for Location object
    }

`config.txt` contains sequence of commands to connect to NB network.

    {
        "command": -  AT command
        "delay": - delay after command execution
        "retries": - number of retries when modem returned not OK
        "purge": - 1 - clean modem buffer after command execution, 0 - do not clean
    }

Known issues
------------

1. If you couldn't flash your code and have message like bellow:

        avrdude: ser_open(): can't open device "\\.\COM15": The system cannot find the file specified.

        
        avrdude done.  Thank you.


        *** [upload] Error 1

        ================================================== [ERROR] Took 13.75 seconds ==================================================

        The terminal process terminated with exit code: 1

it means that COM port is busy by some program. Usually it happenes when serial monitor was not closed.
If you still have this issue you can try to disable and enable required port in "Device Manager".

2. After flashing code you can see just 4 string on display. It means that in `Adafruit_SSD1306.h` (`C:\Users\user\.platformio\lib\Adafruit SSD1306 128x64_ID1513`) `#define SSD1306_128_32` mode is chosen.
You should change it to `#define SSD1306_128_64`.
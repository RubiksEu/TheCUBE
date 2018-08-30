# The CUBE
The Rapid IoT Use Case Development Device

#Hardware Characteristics
- 1. CUBE 4.1 PIR Version
- 2. CUBE 4.1 GPS Version

#Software Version Comments
Test firmware for CUBE version 4.1
2 threads:
- 1. Main thread for OLED display
- 2. Connectivity Thread for sending data to the cloud

#TODO
- Add SD card access
- Add GPS data
- Add PIR data

#KNOWN BUGS
- Sometimes the program hangs due an interrupt issue. Hard restart is needed

#Arduino IDE Installation
https://www.tech4u.info/product-info/IoC/IoC-104-CATM1-R2/Cube3_HW_Package_II__TECH-18-20395-EN-R1.pdf


#Required Libraries (install via Arduino studio):
- Using library Sodaq_UBlox_GPS-master at version 0.9.5 in folder: /path/Documents/Arduino/libraries/Sodaq_UBlox_GPS-master 
- Using library Adafruit_NeoPixel at version 1.1.3 in folder: /path/Documents/Arduino/libraries/Adafruit_NeoPixel 
- Using library Adafruit_Sensor-master at version 1.0.2 in folder: /path/Documents/Arduino/libraries/Adafruit_Sensor-master 
- Using library LSM303 at version 3.0.1 in folder: /path/Documents/Arduino/libraries/LSM303 
- Using library SPI at version 1.0 in folder: /Arduino15/packages/SAMD21-tech4u/hardware/samd/1.0.2/libraries/SPI 
- Using library SD at version 1.2.1 in folder: /path/Documents/Arduino/libraries/SD 
- Using library Scheduler at version 0.4.4 in folder: /path/Documents/Arduino/libraries/Scheduler 
- Using library Adafruit-GFX-Library at version 1.2.2 in folder: /path/Documents/Arduino/libraries/Adafruit-GFX-Library 
- Using library Adafruit_SSD1306 at version 1.1.2 in folder: /path/Documents/Arduino/libraries/Adafruit_SSD1306 


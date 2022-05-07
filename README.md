# ESP8266-WIFI-Module
Hi and welcome to my Github. This is how I setup the ESP8266-WIFI-module for controlling a power outlet. 
This is convenient for controlling the lights at home or shutting off the power to devices with timing. 

## Complete BOM
* EPS8266-WIFI-Modul
* SSH
* A Power outlet
Disclaimer: I take no responsibility for any faults that might oquer. 

Setting up the WIFI:
```c
const char* ssid     = "<ENTER YOUR WIFI SSID>";     // The SSID (name) of the Wi-Fi network you want to connect to
const char* password = "<ENTER YOUR PASSWORD>";      // The password of the Wi-Fi network   
const long utcOffsetInSeconds = 3600 * 2;            //DK tids-zone, Use this link to se you timezone,
const int Lights = 4;                                 // GPIO4 D2 (SDA), use this link ti see the pinout
```



# For the not so advanced user
see `/how to setup the poweroutlet` or `/how to setup the Arduine IDE for ESP8266` for a furter elaboration on the setup. 

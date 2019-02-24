# ESP8266_Multi_Sensor
Temperature, Humidity, Light Level and PIR Motion senor working of a ESP8266 ESP12-E Wifi chip

# Description
I've has a few of these sensors up and running for a while now, but alas one went faulty which gave me the excuse to reprogram the new and existing sensors.  
With my previous sensors, they would occasionally fail to reconnect to WiFi if the router took to long to boot after a power outage. Using the [WiFi Manager]() has cured this problem by restarting the chip if needed and also allows for some configuration in the access point portal so the same code can be used for different sensor setups.  
Also due to the number of devices connecting and updating to my Openhab home automation server, I occasionally had latency issues with everything updating every second.  
In this code I have gone away from updating every second to only updating when values change and a resend after so many minutes of no updates so that I can see if a sensor stops responding.  

## Parts used:
    - ESP8266 ESP-12E Development Board
    - DHT11 or DHT22 Temperature Humidity Sensor
    - Photoresistor GL5528 LDR Photo Resistors Light-Dependent
    - HC-SR501 Human Sensor Module Pyroelectric Infrared PIR Motion Detector Module

## Configuration
Some of the variables in the script can also be set through the Access Point configuration page to allow the code to be used with different setups
```
/* Configurable within WiFi Access Point */
bool hasTemperature = true;
bool hasHumidity = true;
bool hasLight = true;
bool hasPIR = true;
/* Configurable within WiFi Access Point */
char SensorName[25] ; 
char mqtt_server[16] = "192.168.1.200"; 
```

Other variables for confuration are located near the top of the file.  
I have added calibration times to the script to allow sensors to wake up and settle before they are read.  
The PIR calibration is initialized on main power up as it is perminatly powered and the others are from when they are powered up in the main program loop.  
Bare in mind that all but the PIR are not powered on until *THL_rate_limit* has elapsed so the actual sample will take place after *THL_rate_limit + ???CalibrationTimeMS*.  

```
/* Sensor checking & publishing rates */
const int pirCalibrationTimeMS = 30000; //at least 30secs
const int ldrCalibrationTimeMS = 300;
const int dhtCalibrationTimeMS = 1500;
const int PIR_rate_limit = 500; //limit checking to once every ?? ms
const int THL_rate_limit = 60000; //interval in msecs to check temp,humidity,light & only send  if different
const int maxMinutesBetweenSends = 3;

/* Input Pins */
const int thPin = 4;//D2 //temp, humidity pin io 4 has internal pullup
const int pirPin = 5;//D1 //pin for pir
/* Power Pins 3.3v, no pwr pin for PIR as its 5v */
const int ldrPwr = 10;
const int thPwr = 2;
```
## Thanks
Many thanks to the authors and contibutors for the main libraries that made this project possible  
* [PubSubClient](https://github.com/knolleary/pubsubclient) for MQTT publishing
* [DHT](https://github.com/adafruit/DHT-sensor-library) for reading Temperature & Humidity sensors
* [WiFiManager](https://github.com/tzapu/WiFiManager)
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson)

Stuart Blair (smurf0969)

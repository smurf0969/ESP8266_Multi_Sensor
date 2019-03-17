# ESP8266_Multi_Sensor [![Build Status](https://travis-ci.com/smurf0969/ESP8266_Multi_Sensor.svg?branch=master)](https://travis-ci.com/smurf0969/ESP8266_Multi_Sensor) [![Latest Release](https://img.shields.io/github/release/smurf0969/ESP8266_Multi_Sensor.svg)](https://github.com/smurf0969/ESP8266_Multi_Sensor/releases/latest)  
Temperature, Humidity, Light Level and PIR Motion senor working of a ESP8266 ESP12-E Wifi chip

# Overview
I've has a few of these sensors up and running for a while now, but alas one went faulty which gave me the excuse to reprogram the new and existing sensors.  
With my previous sensors, they would occasionally fail to reconnect to WiFi if the router took to long to boot after a power outage. Using the [WiFi Connect](https://github.com/smurf0969/WiFiConnect) has cured this problem by restarting the chip if needed and also allows for some configuration in the access point portal so the same code can be used for different sensor setups.  
Also due to the number of devices connecting and updating to my Openhab home automation server, I occasionally had latency issues with everything updating every second.  
In this code I have gone away from updating every second to only updating when values change and a resend after so many minutes of no updates so that I can see if a sensor stops responding.  

## Parts used:
    - ESP8266 ESP-12E Development Board
    - DHT11 or DHT22 Temperature Humidity Sensor
    - Photoresistor GL5528 LDR Photo Resistors Light-Dependent
    - HC-SR501 Human Sensor Module Pyroelectric Infrared PIR Motion Detector Module


## Configuration & IDE Setup
Please consult/review the [**Wiki**](https://github.com/smurf0969/ESP8266_Multi_Sensor/wiki)

## Thanks
Many thanks to the authors and contibutors for the main libraries that made this project possible  
* [PubSubClient](https://github.com/knolleary/pubsubclient) for MQTT publishing
* [DHT](https://github.com/adafruit/DHT-sensor-library) for reading Temperature & Humidity sensors
* [WifiConnect](https://github.com/smurf0969/WiFiConnect)
* [ArduinoJson](https://github.com/bblanchon/ArduinoJson)

Stuart Blair (smurf0969)

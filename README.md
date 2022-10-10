# Smart Grower
### This repository contains the code for a DIY smart plant grower using a ESP8266
---
### Index
1. [Main features](https://github.com/BHAEK/smart_grower/edit/main/README.md#main-features)
2. [Components used](https://github.com/BHAEK/smart_grower/edit/main/README.md#components-used)
3. [Scheme](https://github.com/BHAEK/smart_grower/blob/main/README.md#scheme)
---
### Main features:
* The system is controlled/monitored using the MQTT protocol
* It supports three different soil moisture levels selected by the user
  * Soil moisture is detected using a "soil moisture sensor" 
  * Soil moisture is automatically controlled using a water pump to irrigate
* It supports three different light levels selected by the user
  * Light intensity is detected using a photoresistor 
  * Light intensity is automatically controlled using "grow full spectrum LEDs"
* The water level in the tank is monitored by a "soil moisture sensor" used as water-level sensor
---
### Software used:
* [TaskScheduler] (https://github.com/arkhipenko/TaskScheduler) 
* [ESP8266WiFi](https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi)
---
### Components used:
* 1 x NodeMCU ESP8266
* 2 x soil moisture sensor
* 2 x full spectrum LEDs
* 1 x photoresistor
* 1 x 5v water pump
* 1 x 1 channel relay module
* 2 x NPN transistor
* 3 x 1KΩ resistors
* 3 x diodes
---
### Scheme:
![Scheme](https://github.com/BHAEK/smart_grower/blob/main/smart-grower_schem.jpg?raw=true)
Scheme realized using [Fritzing software](https://fritzing.org/)

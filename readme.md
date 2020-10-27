# Electronic Label

This project provides an electronic label eg. for conference rooms to show next meeting details.

Communication structure is based on ESP32 connected to Wifi containing the configured MQTT-broker. On this MQTT-broker temperature and humidity measured by DHT22 sensor is stored. Furthermore the broker is used to control the layout and content of the display. 

# Usage

## Build and deploy

 - Import the project in VS-Code (with platformio plugin).
 - copy myconfig.ini_example to myconfig.ini and change to your needs
 - run upload task and watch serial output (this is where all debug messages are shown)

## configuration

 - currently most configuration is still hardcoded in main.cpp :(
 - install mqtt-explorer, provide mqtt-broker eg. mosquitto
 - check for a wifi called esp32ap and connect to it by using default password (12345678)
 - configure esp32 to login to your WIFI containg a mqtt-broker by webinterface (default ip: 172.217.28.1)
 - configure esp32 at topic "sensors/<esp32-mac>/config"
 - check esp32 has published sensor data in mqtt topic "pasx/sensordata/<configured-alias>/data"
 - provide a layout in mqtt-broker eg. to display sensor data at topic "layouts/<layout-name>"

## configuration definition

at topic: "sensors/<esp32-mac>/config"

 {
   "publishTemperature": true,
   "publishHumidity": true,
   "publishRssi": true,
   "dataTopic": "pasx/sensordata/sensor42/data",
   "layoutName": "standard",
   "alias": "sensor42"
 }

## layout definition

at topic: "layouts/<layout-name>"
eg "layouts/standard"

 [
    {
       "type":"rightText",
       "y":2,
       "size":18,
       "color":"black",
       "text":"test$id$?"
    },
    {
       "type":"leftText",
       "y":48,
       "size":9,
       "color":"black",
       "text":"Prod: $prod$"
    },
    {
       "type":"hline",
       "y":40,
       "w":4,
       "color":"black"
    }
 ]

# Used Parts:

ESP-32S ESP-WROOM-32 
https://de.aliexpress.com/item/32864722159.html?spm=a2g0s.9042311.0.0.78bf4c4d8wRSPD
https://www.reichelt.de/de/de/nodemcu-esp32-wifi-und-bluetooth-modul-debo-jt-esp32-p219897.html?r=1

GDEH0213B73 2,13 zoll 250x122 e-papier display with demo kit
https://de.aliexpress.com/item/4000048359196.html?spm=a2g0s.9042311.0.0.24294c4d661JFs

DHT22 digitale temperatur and humidity sensor AM2302 Modul + PCB with cables
https://de.aliexpress.com/item/33037061522.html?spm=a2g0s.9042311.0.0.24294c4d661JFs

# Connect Hardware

## Connect Display:

(see epaper.h for board dependend reconfiguration)
BUSY                ->  PIN_BUSY -> eg. GPIO4
RES                 ->  PIN_RST  -> eg. GPIO16 (RX2)
D/C                 ->  PIN_DC   -> eg. GPIO17 (TX2)
CS                  ->  PIN_CS   -> eg. GPIO5 (VSPI-CS0)
SCK                 ->  PIN_CLK  -> eg. GPIO18 (VSPI-CLK)
SDI(Serial Data In) ->  PIN_DIN  -> eg. GPIO23 (VSPI-MOSI)
GND
3.3V 

## Connect DHT22 Sensor

defined in main.cpp
3.3V
DHT_PIN          21
GND

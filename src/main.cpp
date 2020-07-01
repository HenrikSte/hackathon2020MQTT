#if defined(ARDUINO_ARCH_ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  typedef WebServer WiFiWebServer;
#endif


#include <AutoConnect.h>
#include <WiFi.h>
#include <MQTT.h>
#include "DHTesp.h"
#include "ota.h"
#include <ezTime.h>
#include <RunningMedian.h>
//#include "FS.h" 
//#include <SPIFFS.h>


#include "epaper.h"

ePaper e;

#define minimumPublishInterval 5000
#define signOfLiveInterval  60000
#define timeSyncInterval 60 * 60 * 1000

//RFC3339_EXT  2020-06-09T14:13:44.927-00:00  
//RFC3339      2020-06-09T14:13:44-00:00  
//ISO8601,     2020-06-09T14:18:50-0000
#define myTimeFormat ISO8601


#define LED_BUILTIN_PIN   2
#define DHT_PIN          21

#ifndef MQTT_PW
  #define MQTT_PW ""
#endif

#ifndef MQTT_USER
  #define MQTT_USER ""
#endif

bool ledStatus = true;


WiFiWebServer server;
WiFiClient    wifiClient;

AutoConnect portal(server);
AutoConnectConfig config;

DHTesp dht;
const unsigned long dhtReadInterval = 5000; 
const double        dhtToleranceTemp = 0.2; 
const double        dhtToleranceHum  = 0.5; 
const double        rssiTolerance    = 10; 

RunningMedian temperatureSamples = RunningMedian(5);
RunningMedian humiditySamples    = RunningMedian(5);


MQTTClient MQTTclient(4096);

bool publishTemperature = true;
bool publishHumidity    = true;
bool publishRssi        = false;

#define dataTopicPrefix   "pasx/sensordata/" + myAlias + "/data/"
#define configTopicPrefix  "sensors/" + WiFi.macAddress() + "/config/"

const char* dataPrefixPlaceHolder   = "$dataPrefix$";
const char* configPrefixPlaceHolder = "$configPrefix$";
const char* subscribedTopics [] = { "$configPrefix$config", "$configPrefix$signOfLife"  };
enum enumSubscribedTopics         {  stConfig,               stSignOfLife,             stLast };

const char* publishedTopics [] = { "$configPrefix$signOfLife", "$dataPrefix$temperature", "$dataPrefix$humidity", "$dataPrefix$rssi"};
enum  enumPublishedTopics         { ptSignOfLife,               ptTemperature,             ptHumidity,             ptRssi,                 ptLast};


const char* layoutBaseTopic = "layouts/";

String layoutName;      // name (topic = prefix + name) of Layout
String newLayoutName;   // set in MessageRecieved, used in loop
String layout;          // actual layout data as JSON
bool   layoutNameHasChanged = false;   


String dataTopic;      // where to pick up data 
String newDataTopic;   // set in MessageRecieved, used in loop
bool   dataTopicHasChanged = false;   
String data;  // actual data as json

String myAlias;

bool dataHasChanged = false;
bool layoutHasChanged      = false;


void toggleLed()
{ 
  ledStatus = !ledStatus;
  digitalWrite(LED_BUILTIN_PIN, ledStatus);
}


void autoConnect()
{
    // Responder of root page and apply page handled directly from WebServer class.
  server.on("/", []() {
  String content = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
Place the root page with the sketch application.&ensp;
__AC_LINK__
</body>
</html>
    )";
    content.replace("__AC_LINK__", String(AUTOCONNECT_LINK(COG_16)));
    server.send(200, "text/html", content);
  });
  config.ota = AC_OTA_BUILTIN;
  portal.config(config);
  portal.begin();

}

String removePrefix(const String& fullTopic)
{
  String topic = fullTopic;
  topic.replace(configTopicPrefix,"");
  topic.replace(configPrefixPlaceHolder,"");
  topic.replace(dataTopicPrefix,"");
  topic.replace(dataPrefixPlaceHolder,"");
  return topic;
}

String addPrefix(const String& topic)
{
  String fullTopic;
  fullTopic = String(topic);
  fullTopic.replace(dataPrefixPlaceHolder, dataTopicPrefix);
  fullTopic.replace(configPrefixPlaceHolder, configTopicPrefix);
  return fullTopic;
}

bool unsubscribe(const String& topic)
{
  String fullTopic = addPrefix(topic);
  Serial.print("UNSUBscribing ");
  Serial.print(fullTopic);
  bool success = MQTTclient.unsubscribe(fullTopic);
  if (!success)
  {
    Serial.println(" unsub FAILED");
  }
  else
  {
    Serial.println(" ok");
  }
  return success;
}

bool subscribe(const String& topic)
{
  String fullTopic = addPrefix(topic);
  Serial.print("Subscribing ");
  Serial.print(fullTopic);

  bool success = MQTTclient.subscribe(fullTopic);
  if (!success)
  {
    Serial.println(" sub FAILED");
  }
  else
  {
    Serial.println(" ok");
  }
  return success;
}



bool subscribeDataTopic( const String& newDataTopic)
{
  bool success = true;

  if (newDataTopic != dataTopic)
  {
    if (dataTopic.length())
    {
      unsubscribe(dataTopic);
      dataTopic = "";
    }
    
    if (newDataTopic.length())
    {
      if (!subscribe(newDataTopic))
      {
        success = false;
      }
      else
      {
        dataTopic = newDataTopic;
      }
    }
  }
  return success;
}



bool subscribeLayoutTopic(const String& newLayoutName)
{
  bool success = true;

  if (newLayoutName != layoutName)
  {
    String topic;
    if (layoutName.length())
    {
      topic = layoutBaseTopic + layoutName;
      unsubscribe(topic);
      layoutName = "";
    }
    if (newLayoutName.length())
    {
      topic = layoutBaseTopic + newLayoutName;
      if (!subscribe(topic))
      {
        success = false;
      }
      else
      {
        layoutName = newLayoutName;
      }
    }
  }
  return success;
}

bool subscribeTopics() 
{
  bool success = true;
  Serial.println("Subscribing topic list:");
  for (int i=0; i<stLast; i++)
  {
    if (!subscribe(subscribedTopics[i]))
    {
      success = false;
    }
  }
  
  success = success 
         && subscribeLayoutTopic(layoutName)  // initially blank!
         && subscribeDataTopic(dataTopic); 

  return success;

}

void connect(bool reconnecting=false)
{
  if (reconnecting)
  {
    Serial.println("** reconnecting! **");
  }

  Serial.print("checking wifi...");
  if (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print(" reconnecting ");
    WiFi.reconnect();
  }

  if (WiFi.status() == WL_CONNECTED) 
  {
    Serial.println(" OK");

    Serial.print("checking MQTT client: ");
    if (MQTTclient.connected())  
    {
      
      Serial.println(" OK");
    }
    else
    {
      Serial.print(" connecting MQTT client:");
      if (MQTTclient.connect(WiFi.macAddress().c_str(), MQTT_USER , MQTT_PW))
      {
        Serial.println(" OK");
        subscribeTopics(); 
      }
      else
      {
        Serial.println(" connect failed");
      }
    }
  }
  else
  {
    Serial.println("wifi failed");
  }

}

bool checkConnection()
{
  if (!MQTTclient.connected())
  {
    connect(true);
  }
  return MQTTclient.connected();
}






void messageReceived(String &topic, String &payload) 
{
  Serial.println("Incoming: " + topic + " - >" + payload + "<");

  if (removePrefix(topic) == removePrefix(subscribedTopics[stConfig]))
  {
    Serial.println("** new config! ");

    /*
{
  "publishTemperature": true,
  "publishHumidity": true,
  "publishRssi": true,
  "dataTopic": "pasx/equipment/BC40N01",
  "layoutName": "standard",
  "alias": "sensor42"
}
    */

    DynamicJsonDocument doc(512);
    deserializeJson(doc, payload);
    JsonObject obj = doc.as<JsonObject>();
    publishTemperature    = obj["publishTemperature"];
    publishHumidity       = obj["publishHumidity"];
    publishRssi           = obj["publishRssi"];
    String _newDataTopic  = obj["dataTopic"];
    String _newLayoutName = obj["layoutName"];
    String newAlias       = obj["alias"];

    if (newAlias.length())
    {
      myAlias = newAlias;
    }

    // async handling of dynamic subscibes:

    if (_newDataTopic.length())
    {
      newDataTopic = _newDataTopic;
      dataTopicHasChanged = true;
    }

    if (_newLayoutName.length())
    {
      newLayoutName = _newLayoutName;
      layoutNameHasChanged = true;
    }
 
    
  }
  else if (topic == dataTopic)
  {
    Serial.println("** new Data! ");
    // e.showText(font9, payload.c_str());
    dataHasChanged = true;
    data = payload;
  }
  else if (removePrefix(topic) == removePrefix(layoutBaseTopic + layoutName))
  {
    Serial.println("** new Layout! ");
    // e.showText(font9, payload.c_str());
    layout = payload;
    layoutHasChanged = true;
  }
  else if (removePrefix(topic) == removePrefix(subscribedTopics[stSignOfLife]))
  {
    Serial.println("** SignOfLife: ");
  }

}

bool publishString(const char* topic, const char* s)
{
  bool success = true;
  String fullTopic = addPrefix(topic);

  Serial.print("publishing ");
  Serial.print(fullTopic);
  Serial.print(" as ");
  Serial.print(s);


  if (fullTopic.indexOf("//") < 0) // if myAlias is not set we will have a topic//topic. Bit of a hack...
  {
    checkConnection(); // vermutlich überflüssig

    success = MQTTclient.publish(fullTopic, s, true, 0);
    if (!success)
    {
      Serial.print(" publish FAILED ");
    }
    else
    {
      Serial.println(" ok");
    }
  }
  else
  {
    Serial.println(" ok, but my ALIAS not set");
  }

  toggleLed();
  return success;
}

bool publishDouble(const char* topic, double value, const char* UOM)
{
  String s;
  bool success= false;

  StaticJsonDocument<512> doc;

  // Set the values in the document
  doc["value"] = String(value); // Wunsch von Philipp: Zahlen auch als String
  doc["uom"] = UOM;
  doc["timestamp"] = UTC.dateTime(myTimeFormat); 

  if (serializeJsonPretty(doc,s) ==0)
  {
    Serial.println(F("publishDouble: Failed to serialize JSON"));
  }
  else
  {
    success = publishString(topic, s.c_str());
  }
  return success;
}

void syncTime()
{
  Serial.print("Connecting NTP server... ");
  #ifdef MY_NTP_SERVER
    Serial.print(MY_NTP_SERVER);
    ezt::setServer(MY_NTP_SERVER);
  #else
    Serial.print(NTP_SERVER);
  #endif

	waitForSync();
	Serial.println("UTC: " + UTC.dateTime());
}


void setup() 
{
  Serial.begin(115200);
  pinMode (LED_BUILTIN_PIN, OUTPUT);
  
  delay(500);
  Serial.println();
  #ifdef ESP32DRIVERBOARD
    Serial.print("Compiled for driverboard");
  #else
    Serial.print("Compiled for regular ESP32");
  #endif

  #if defined BWR_DISPLAY
    Serial.println(" with BWR display");
  #else
    Serial.println(" with BW display");
  #endif




  Serial.println("Starting dht...");
	dht.setup(DHT_PIN, DHTesp::DHT22);

  Serial.println("AutoConnect Wifi...");
  autoConnect();
  Serial.print("My MAC: "); 
  Serial.println(WiFi.macAddress().c_str());
  Serial.print("My IP: "); 
  Serial.println(WiFi.localIP());

  syncTime();

  Serial.print("Connecting MQTT to ");
  Serial.println(MQTT_SERVER);
  MQTTclient.begin(MQTT_SERVER, wifiClient);
  MQTTclient.onMessage(messageReceived);
  
  connect();

  Serial.print("Setting up OTA...: ");
  setupOTA("TemplateSketch");

  Serial.println("Init Display: ");
  e.display.init(); // enable diagnostic output on Serial
#if defined ESP32DRIVERBOARD
  SPI.end(); 
  SPI.begin(13, 12, 14, 15);
  //SPI.begin(PIN_CLK,PIN_MISO,PIN_MOSI, PIN_SS);
#endif

  Serial.println("Render Label...");
  //e.renderLabel("","");
  e.splashScreen();

  
  Serial.println("\n**** Setup() complete. ****\n");
}



void loop() 
{
//  static unsigned long    startMillis = millis();
  static unsigned long  nextDhtRead = 0;
  static float          oldTemperature;
  static float          oldHumidity;
  static uint8_t        oldRssi;
  static unsigned long    nextPublish[ptLast] = {0,0,0,0};
  
  static unsigned long nextTimeSync = timeSyncInterval;
  
  //Web Interface
  portal.handleClient();

  // Over the air update
  ArduinoOTA.handle();
 

  if ((publishTemperature || publishHumidity) 
      && millis() >= nextDhtRead)
  {
    if (dht.getStatus() == 0)
    {
      TempAndHumidity v = dht.getTempAndHumidity();
      nextDhtRead       = millis() + dhtReadInterval;
      temperatureSamples.add(v.temperature);
      humiditySamples.   add(v.humidity);
    }
    else
    {
      Serial.println("DHT error status: " + String(dht.getStatusString()));
      nextDhtRead       = millis() + dhtReadInterval;
    }
  }


  // handle MQTT Client subscriptions
 

  if (MQTTclient.connected()) 
  {
    MQTTclient.loop();
     
    // MQTT Publish

    if (publishTemperature)
    {
      if  (millis() >= nextPublish[ptTemperature]) 
      {
        float newTemperature = temperatureSamples.getMedian();
        if (publishTemperature && abs(newTemperature - oldTemperature) > dhtToleranceTemp)
        {
          publishDouble(publishedTopics[ptTemperature], newTemperature, "C");
          nextPublish[ptTemperature] = millis() + minimumPublishInterval;
          oldTemperature = newTemperature;
        }
      }
    }

    if  (publishHumidity)
    {
      if  (millis() >= nextPublish[ptHumidity]) 
      {
        float newHumidity = humiditySamples.getMedian();
        if (publishHumidity && abs(newHumidity - oldHumidity)>dhtToleranceHum)
        {
          publishDouble(publishedTopics[ptHumidity], newHumidity, "RH%");
          nextPublish[ptHumidity] = millis() + minimumPublishInterval;
          oldHumidity = newHumidity;
        }
      }
    }

    if (publishRssi && millis()>nextPublish[ptRssi])
    {
      uint8_t rssi= WiFi.RSSI();
      if (abs(oldRssi-rssi)>rssiTolerance) 
      {
        publishDouble(publishedTopics[ptRssi], rssi, "dB");
        nextPublish[ptRssi] = millis() + minimumPublishInterval;
        oldRssi = rssi;
      }
    }

    if (millis()>nextPublish[ptSignOfLife])
    {
        publishString(publishedTopics[ptSignOfLife], UTC.dateTime(myTimeFormat).c_str());
        nextPublish[ptSignOfLife] = millis() + signOfLiveInterval;
    }
  }
  else
  {
    //int todo; // error handling
    // happens in next loop interation
  }

  if ((layoutHasChanged || dataHasChanged)
   && (layout.length() && data.length()))
  {
    
    e.renderLabel(data, layout);
    layoutHasChanged = false;
    dataHasChanged = false;
  }

  if (dataTopicHasChanged)
  {
    subscribeDataTopic(newDataTopic);     // dynamic subscribes
  }
  if (layoutNameHasChanged)
  {
    subscribeLayoutTopic(newLayoutName);  //
  }

  if (millis()>nextTimeSync)
  {
    syncTime();
    nextTimeSync = millis() + nextTimeSync;
  }

}
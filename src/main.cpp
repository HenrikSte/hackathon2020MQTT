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
//#include "FS.h" 
//#include <SPIFFS.h>

#define MQTT_SERVER "192.168.178.55"
#define MQTT_USER   ""
#define MQTT_PW     ""
#define topicPrefix WiFi.macAddress() + "/"
#define minimumPublishInterval 5000
#define signOfLiveInterval  60000
#define timeSyncInterval 60 * 60 * 1000

//RFC3339_EXT  2020-06-09T14:13:44.927-00:00  
//RFC3339      2020-06-09T14:13:44-00:00  
//ISO8601,     2020-06-09T14:18:50-0000
#define myTimeFormat ISO8601


#define LED_BUILTIN_PIN   2
#define DHT_PIN          21

bool ledStatus = true;

bool publishTemperature = false;
bool publishHumidity    = false;
bool publishRssi        = false;


WiFiWebServer server;
WiFiClient    wifiClient;

AutoConnect portal(server);
AutoConnectConfig config;

DHTesp dht;
const unsigned long dhtReadInterval = 5000; 
const double        dhtToleranceTemp = 0.2; 
const double        dhtToleranceHum  = 0.5; 
const double        rssiTolerance    = 10; 


MQTTClient MQTTclient;

const char* subscribedTopics [] = { "signOfLife",  "command/config" };
enum enumSubscribedTopics         { stSignOfLife,  stConfig,         stLast };

const char PROGMEM configFileName[]  = "/config.json";


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
  String topic( fullTopic);
  topic.replace(topicPrefix,"");
  return topic;
}

String addPrefix(const String& topic, bool withPrefix)
{
  String fullTopic;
  if (withPrefix)
  {
    fullTopic = String(topicPrefix) +  topic;
  }  
  else
  {
    fullTopic = topic;
  }
  return fullTopic;
}

void unsubscribe(const String& topic, bool withPrefix)
{
  String fullTopic = addPrefix(topic, withPrefix);
  Serial.print("UNSUBscribing ");
  Serial.println(fullTopic);
  MQTTclient.unsubscribe(fullTopic);
}

void subscribe(const String& topic, bool withPrefix)
{
  String fullTopic = addPrefix(topic, withPrefix);
  Serial.print("Subscribing ");
  Serial.println(fullTopic);
  MQTTclient.subscribe(fullTopic);
}

void subscribeTopics() 
{
  for (int i=0; i<stLast; i++)
  {
    subscribe(subscribedTopics[i], true);
  }
}



void connect(bool reconnecting=false)
{
  if (reconnecting)
  {
    Serial.println("** reconnecting! **");
    int todo; //reconnect procedure, e.g. fetching values
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
        Serial.println(" failed");
      }
    }
   }
  else
  {
    Serial.println(" failed");
  }

}

void messageReceived(String &topic, String &payload) 
{
  if (removePrefix(topic) == subscribedTopics[stConfig])
  {
    Serial.println("** config: " + topic + " - >" + payload + "<");
    DynamicJsonDocument doc(512);
    deserializeJson(doc, payload);
    JsonObject obj = doc.as<JsonObject>();
    publishTemperature = obj["publishTemperature"];
    publishHumidity    = obj["publishHumidity"];
    publishRssi        = obj["publishRssi"];
   
    Serial.println("");
    serializeJson(doc, Serial);

    Serial.print("thr: ");
    Serial.print(publishTemperature);
    Serial.print(publishHumidity);
    Serial.println(publishRssi);
  
  }
  else
  {
    Serial.println("incoming: " + topic + " - " + payload);
    int todo; // do something useful
  }
}

void publishString(const char* topic, const char* s, bool withPrefix=true)
{
  String fullTopic = addPrefix(topic, withPrefix);
  Serial.print("publishing ");
  Serial.print(fullTopic);
  Serial.print(" as ");
  Serial.println(s);
  MQTTclient.publish(fullTopic, s, true, 0);
  toggleLed();
}

void publishDouble(const char* topic, double value, const char* UOM, bool withPrefix = true)
{
  String s;

  StaticJsonDocument<512> doc;

  // Set the values in the document
  doc[topic] = value;
  doc["UOM"] = UOM;
  doc["timestamp"] = UTC.dateTime(myTimeFormat); 

  if (serializeJsonPretty(doc,s) ==0)
  {
    Serial.println(F("publishDouble: Failed to serialize JSON"));
  }
  else
  {
    publishString(topic, s.c_str(), withPrefix);
  }

}

void syncTime()
{
  Serial.print("Connecting NTP server... ");
	waitForSync();
	Serial.println("UTC: " + UTC.dateTime());
}

void setup() 
{
  Serial.begin(115200);
  pinMode (LED_BUILTIN_PIN, OUTPUT);
  
  delay(500);
  Serial.println();

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
  
  Serial.println("\n**** Setup() complete. ****\n");
}

void loop() 
{
//  static unsigned long    startMillis = millis();
  static unsigned long    nextDhtRead = 0;
  static TempAndHumidity  oldValues;
  static uint8_t          oldRssi;
  enum  publishedTopics  { ptDht, ptRssi, ptSignOfLife, ptPublishCount};
  static unsigned long    nextPublish[ptPublishCount] = {0,0,0,};

  static unsigned long nextTimeSync = timeSyncInterval;
  
  //Web Interface
  portal.handleClient();

  // Over the air update
  ArduinoOTA.handle();
 
  // handle MQTT Client subscriptions
  if (!MQTTclient.connected()) 
  {
    connect(true);
  }

  if (MQTTclient.connected()) 
  {
    MQTTclient.loop();
     
    // MQTT Publish

    if (publishTemperature || publishHumidity)
    {
      if  (millis() >= nextPublish[ptDht]
        && millis() >= nextDhtRead) 
      {

        if (dht.getStatus() == 0) 
        {
          TempAndHumidity newValues = dht.getTempAndHumidity();
          nextDhtRead       = millis() + dhtReadInterval;

          if (publishTemperature && abs(newValues.temperature-  oldValues.temperature) > dhtToleranceTemp)
          {
            publishDouble("temperature", newValues.temperature, "C");
            nextPublish[ptDht] = millis() + minimumPublishInterval;
            oldValues.temperature = newValues.temperature;
          }

          if (publishHumidity && abs(newValues.humidity - oldValues.humidity)>dhtToleranceHum)
          {
            publishDouble("humidity", newValues.humidity, "RH%");
            nextPublish[ptDht] = millis() + minimumPublishInterval;
            oldValues.humidity = newValues.humidity;
          }
        }
        else
        {
          Serial.println("DHT error status: " + String(dht.getStatusString()));
        }
      }
    }

    if (publishRssi && millis()>nextPublish[ptRssi])
    {
      uint8_t rssi= WiFi.RSSI();
      if (abs(oldRssi-rssi)>rssiTolerance) 
      {
        publishDouble("RSSI", rssi, "dB");
        nextPublish[ptRssi] = millis() + minimumPublishInterval;
        int todo; // make a publish int method
        oldRssi = rssi;
      }
    }

    if (millis()>nextPublish[ptSignOfLife])
    {
        publishString(subscribedTopics[stSignOfLife], UTC.dateTime(myTimeFormat).c_str(), true);
        nextPublish[ptSignOfLife] = millis() + signOfLiveInterval;
    }
  }
  else
  {
    int todo; // error handling
  }

  if (millis()>nextTimeSync)
  {
    syncTime();
    nextTimeSync = millis() + nextTimeSync;
  }

}
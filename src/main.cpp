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

#if defined henrik_at_home
  #define MQTT_SERVER "192.168.178.55"
  #define MQTT_USER   ""
  #define MQTT_PW     ""
#else
  #define MQTT_SERVER "pasx-team-erp.werum.net"
  #define MQTT_USER   "pasx"
  #define MQTT_PW     "pasx"
#endif

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

bool publishTemperature = true;
bool publishHumidity    = true;
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

RunningMedian temperatureSamples = RunningMedian(5);
RunningMedian humiditySamples    = RunningMedian(5);


MQTTClient MQTTclient;

const char* subscribedTopics [] = { "signOfLife",  "command/config", "display/text", "display/json" };
enum enumSubscribedTopics         { stSignOfLife,  stConfig,         stDisplayText,  stDisplayJson, stLast };

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
  else if (removePrefix(topic) == subscribedTopics[stSignOfLife])
  {
    Serial.println("** SignOfLife: " + topic + " - >" + payload + "<");
    //showText(font18, payload.c_str());
  }
  else if (removePrefix(topic) == subscribedTopics[stDisplayText])
  {
    Serial.println("** Display Text: " + topic + " - >" + payload + "<");
    e.showText(font18, payload.c_str());
  }
  else if (removePrefix(topic) == subscribedTopics[stDisplayJson])
  {
    Serial.println("** Display Json: " + topic + " - >" + payload + "<");
    DynamicJsonDocument doc(512);
    deserializeJson(doc, payload);
    JsonObject obj = doc.as<JsonObject>();
    String text = obj["text"];
    int    size = obj["size"];
    
    const GFXfont* f;
    switch (size)
    {
      case 9:
        f = font9;
        break;
      case 12:
        f = font12;
        break;
      case 18:
        f = font18;
        break;
      case 24:
        f = font24;
        break;

      default:
        f = font9;
        break;
    };

    e.showText(f, text.c_str());
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
  
  ezt::setServer("172.20.200.22");
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
//#if defined ESP32DRIVERBOARD
  SPI.end(); 
  SPI.begin(13, 12, 14, 15);
  //SPI.begin(PIN_CLK,PIN_MISO,PIN_MOSI, PIN_SS);
//#endif

  
  Serial.println("\n**** Setup() complete. ****\n");
}

void loop() 
{
//  static unsigned long    startMillis = millis();
  static unsigned long    nextDhtRead = 0;
  static float     oldTemperature;
  static float     oldHumidity;
  static uint8_t   oldRssi;
  enum  publishedTopics  { ptDht, ptRssi, ptSignOfLife, ptPublishCount};
  static unsigned long    nextPublish[ptPublishCount] = {0,0,0};

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

  if (millis() >= nextDhtRead)
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

  if (MQTTclient.connected()) 
  {
    MQTTclient.loop();
     
    // MQTT Publish

    if (publishTemperature || publishHumidity)
    {
      if  (millis() >= nextPublish[ptDht]) 
      {
        float newTemperature = temperatureSamples.getMedian();
        if (publishTemperature && abs(newTemperature - oldTemperature) > dhtToleranceTemp)
        {
          publishDouble("temperature", newTemperature, "C");
          nextPublish[ptDht] = millis() + minimumPublishInterval;
          oldTemperature = newTemperature;
        }

        float newHumidity = humiditySamples.getMedian();
        if (publishHumidity && abs(newHumidity - oldHumidity)>dhtToleranceHum)
        {
          publishDouble("humidity", newHumidity, "RH%");
          nextPublish[ptDht] = millis() + minimumPublishInterval;
          oldHumidity = newHumidity;
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
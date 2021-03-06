#include <FS.h>//this needs to be first, or it all crashes and burns...
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#include <SPIFFS.h>
#endif
#include <PubSubClient.h> //https://github.com/knolleary/pubsubclient
#include <stdlib.h>
#include <DHT.h> //https://github.com/adafruit/DHT-sensor-library

#include <WiFiConnect.h>          //https://github.com/smurf0969/WiFiConnect

#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson


#if defined(ESP8266)
extern "C" {
#include "user_interface.h"
}
#define ESP_getChipId()   (ESP.getChipId()) ///< Gets an ID from the chip
#else
#include <esp_wifi.h>
#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())///< Gets an ID from the chip
#endif

#define sbVersion "4.2"
#define clearSavedWiFiConnection false //removes saved connection for testing Access point (true,false)

#define sbDebug  false //disables mqtt and outputs msg to Serial for testing
/* Configurable within WiFi Access Point */
bool hasTemperature = true;
bool hasHumidity = true;
bool hasLight = true;
bool hasPIR = true;
/* Configurable within WiFi Access Point */
char SensorName[25] ; 
char mqtt_server[16] = "192.168.1.200"; 

// Do not start with /
// ideal usage:  myhome/myfloor/myroom/mysensor/whatsensor
const char* tTopic = "sensor/%s/temperature";
const char* hTopic = "sensor/%s/humidity";
const char* bTopic = "sensor/%s/battery";
const char* ldrTopic = "sensor/%s/light";
const char* mTopic = "sensor/%s/motion";
const char* errorTopic = "sensor/%s";


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
const int thPwr = 14;

WiFiClient espClient;
PubSubClient client(espClient);

DHT dht(thPin, DHT11);
//DHT dht(thPin, DHT22);

int snl = 0;//holder for SensorName length

// **************************************************
// **********    WiFiConnect Callbacks    ***********
// **************************************************
bool shouldSaveConfig = false;
void saveConfigCallback() {
  Serial.println(F("Should save config"));
  shouldSaveConfig = true;
}

// **************************************************
// **********      Config Load/Save       ***********
// **************************************************
const int json_doc_size = 250; //make sure its longer than the complete json string
void loadConfig() {
  Serial.print(F("Loading Config: "));
  if (!SPIFFS.begin()) {
    Serial.println(F("Mount FS FAILED!!"));
    return;
  } else {
    //    if(SPIFFS.format()){
    //      Serial.println(F("File System Formatted"));
    //      while(1)delay(1000);/loop forever
    //    }
    if (SPIFFS.exists("/config.json")) {
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println(F("opened config file"));
        size_t sizec = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[sizec]);

        configFile.readBytes(buf.get(), sizec);
        DynamicJsonDocument doc(json_doc_size);
        DeserializationError error = deserializeJson(doc, buf.get());
        if (error) {
          Serial.println(F("\nFailed to load json config"));
        } else {
          //copy saved params
          strcpy(SensorName, doc["SensorName"]);
          strcpy(mqtt_server, doc["mqtt_server"]);
          hasTemperature = doc["hasTemperature"];
          hasHumidity = doc["hasHumidity"];
          hasLight = doc["hasLight"];
          hasPIR = doc["hasPIR"];
        }
        configFile.close();
      } else {
        Serial.println(F("Failed to open config file"));
      }
    } else {
      Serial.println(F("Config file not found."));
      shouldSaveConfig = true; //save the default coded values
    }
    SPIFFS.end();
  }
}
void saveConfig() {
  shouldSaveConfig = false;

  Serial.print(F("Saving config: "));

  if (!SPIFFS.begin()) {
    Serial.println(F("UNABLE to open SPIFFS"));
    return;
  }
  DynamicJsonDocument doc(json_doc_size);

  doc["SensorName"] = SensorName;
  doc["mqtt_server"] = mqtt_server;
  doc["hasTemperature"] = hasTemperature;
  doc["hasHumidity"] = hasHumidity;
  doc["hasLight"] = hasLight;
  doc["hasPIR"] = hasPIR;

  File configFile = SPIFFS.open("/config.json", "w");

  if (!configFile) {
    Serial.println(F("failed to open config file for writing"));
    return;
  }

  serializeJson(doc, configFile);
  Serial.println(F("Saved"));
  serializeJson(doc, Serial);
  Serial.println("");
  configFile.close();
  //end save
  SPIFFS.end();
}
// **************************************************
// **********         PIR Motion          ***********
// **************************************************
bool lastPIR = false;
unsigned long lastPIR_lookup = 0;
long lastSendMotion = 0;
void  processPIR() {
  unsigned long tNow = millis();
  bool sendNow = false;
  if (tNow - lastSendMotion >= (maxMinutesBetweenSends * 60 * 1000))sendNow = true;
  if (hasPIR && (tNow > pirCalibrationTimeMS || sendNow)) {
    if (lastPIR_lookup > 0 && !sendNow) {
      if (tNow - lastPIR_lookup < PIR_rate_limit) {
        return;
      }
    }
    bool vPIR = digitalRead(pirPin);
    lastPIR_lookup = tNow;
    if (vPIR == lastPIR && !sendNow) {
      return;
    }
    lastPIR = vPIR;
    lastSendMotion = tNow;
    char *pirStr = (char*)(vPIR == true ? "1" : "0");
    char mTopicSn[sizeof(mTopic) + snl + 1];
    sprintf(mTopicSn, mTopic, SensorName);
    sendMqtt(mTopicSn, pirStr, true);
  }
}
// **************************************************
// **********           Setup             ***********
// **************************************************

void setup() {
  Serial.begin(115200);
  delay(500);
  sprintf(SensorName, "ESP%d", ESP_getChipId());
  Serial.printf("\nDefault Sensor Name: %s\n", SensorName);
  loadConfig();
  if (shouldSaveConfig) {
    saveConfig(); /*save defaults*/
  }

  //WiFiManager
  WiFiConnectParam custom_SensorName("SensorName", "Sensor name", SensorName, 25);
  WiFiConnectParam custom_mqtt_server("mqtt_server", "MQTT IP", mqtt_server, 16);
  WiFiConnectParam custom_hasTemperature("hasTemperature", "Has Temperature", (hasTemperature ? "true" : "false"), 6);
  WiFiConnectParam custom_hasHumidity("hasHumidity", "Has Humidity", (hasHumidity ? "true" : "false"), 6);
  WiFiConnectParam custom_hasLight("hasLight", "Has LDR", (hasLight ? "true" : "false"), 6);
  WiFiConnectParam custom_hasPIR("hasPIR", "Has PIR", (hasPIR ? "true" : "false"), 6);

  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiConnect wc;
  wc.setSaveConfigCallback(saveConfigCallback);
  wc.addParameter(&custom_SensorName);
  wc.addParameter(&custom_mqtt_server);
  wc.addParameter(&custom_hasTemperature);
  wc.addParameter(&custom_hasLight);
  wc.addParameter(&custom_hasLight);
  wc.addParameter(&custom_hasPIR);

#if clearSavedWiFiConnection
  //reset settings - for testing
  wc.resetSettings();
#endif

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  Serial.printf("\nSensor Name: %s\n", SensorName);
  Serial.print(F("Starting WiFi: "));
  if (!wc.autoConnect(SensorName)) {
    //start the configuration access point
    wc.startConfigurationPortal(AP_RESET,SensorName);

  }



  //if you get here you have connected to the WiFi
  Serial.print("connected ");
  Serial.println(WiFi.localIP());

  if (shouldSaveConfig) {
    strcpy(SensorName, custom_SensorName.getValue());
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    hasTemperature = custom_hasTemperature.getValue();
    hasHumidity = custom_hasHumidity.getValue();
    hasLight = custom_hasLight.getValue();
    hasPIR = custom_hasPIR.getValue();
    saveConfig();
  }

  pinMode(ldrPwr, OUTPUT);
  pinMode(thPwr, OUTPUT);

  digitalWrite(ldrPwr, LOW);
  digitalWrite(thPwr, LOW);

  pinMode(thPin, INPUT_PULLUP);

  pinMode(pirPin, INPUT_PULLUP);
  digitalWrite(pirPin, LOW);

  Serial.println(F("Setting up mqtt client"));
  client.setServer(mqtt_server, 1883);


  Serial.println();  Serial.println(F("### Starting to monitor sensors ###"));  Serial.println();
 
}

// **************************************************
// **********    WiFi/MQTT Reconnector    ***********
// **************************************************
void reconnect() {
  int conAttempt = 1;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf("WiFi not connected, trying to reconnect Attempt: %d\n", conAttempt);
    if (conAttempt <= 5) {
      delay(5000);
      WiFi.begin();
      delay(100);
      conAttempt++;
    } else {
      Serial.println(F("Resetting"));
      delay(1000);
 #ifdef ESP8266
      ESP.reset();
 #else
      ESP.restart();
 #endif
      delay(5000);
      return;
    }
  }
  conAttempt = 1;
  // Loop until we're reconnected
  while (!client.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      reconnect();
      break;
    }
    Serial.print(F("Attempting MQTT connection..."));
    // Attempt to connect
    if (client.connect(SensorName)) {
      Serial.println(F("connected"));
    } else {
      Serial.print(F("failed, rc = "));
      Serial.print(client.state());
      Serial.println(F(" try again in 5 seconds"));
      // Wait 5 seconds before retrying
      for(int x=1;x<=5;x++){
        delay(1000);
        yield();
      }
      conAttempt++;
    }
    if (conAttempt > 5) {
      Serial.println(F("Resetting"));
      delay(1000);
 #ifdef ESP8266
      ESP.reset();
 #else
      ESP.restart();
 #endif
      for(int x=1;x<=5;x++){
        delay(1000);
        yield();
      }
      return;
    }
  }

}
// **************************************************
// **********      Publish to MQTT        ***********
// **************************************************
void sendMqtt(char* topic, char* msg, bool clientLoop) {

#if sbDebug
  Serial.println();
  Serial.println(F("Would send if not debug: "));
  Serial.println(topic);
  Serial.println(msg);
  Serial.println();
  return;
#else
  reconnect();
  //Serial.printf("\nPulishing %s, %s, %d\n\n",topic,msg,clientLoop);
  client.publish(topic, msg);
  if (clientLoop) {
    client.loop();
  }
#endif
  Serial.print("Uptime: ");
  Serial.println(getUptime());
}

// **************************************************
// **********    Humidity Msg Builder     ***********
// **************************************************
long lastSendHumidity = 0;
void sendHumidity(float &vHumidity) {
  lastSendHumidity = millis();
  char hTopicSn[sizeof(hTopic) + snl + 10];
  sprintf(hTopicSn, hTopic, SensorName);
  String humStr = String( vHumidity);
  char *humChar = new char[humStr.length() + 1];
  strcpy(humChar, humStr.c_str());
  sendMqtt(hTopicSn, humChar, true);
}

// **************************************************
// **********  Temperature Msg Builder    ***********
// **************************************************
long lastSendTemperature = 0;
void sendTemperature(float &vTemperature) {
  lastSendTemperature = millis();
  char tTopicSn[sizeof(tTopic)+ snl+10];
  sprintf(tTopicSn, tTopic, SensorName);
  String tempStr = String(vTemperature);
  char *tempChar = new char[tempStr.length() + 1];
  strcpy(tempChar, tempStr.c_str());
  sendMqtt(tTopicSn, tempChar, true);
}

String getUptime(){
  unsigned long ms=millis();
  long days = 0;
  long hours = 0;
  long mins = 0;
  long secs = 0;
  String secs_o = ":";
  String mins_o = ":";
  String hours_o = ":";
  secs = ms / 1000; // set the seconds remaining
  mins = secs / 60; //convert seconds to minutes
  hours = mins / 60; //convert minutes to hours
  days = hours / 24; //convert hours to days
  secs = secs - (mins * 60); //subtract the coverted seconds to minutes in order to display 59 secs max
  mins = mins - (hours * 60); //subtract the coverted minutes to hours in order to display 59 minutes max
  hours = hours - (days * 24); //subtract the coverted hours to days in order to display 23 hours max
  if (secs < 10) {
    secs_o = ":0";
  }
  if (mins < 10) {
    mins_o = ":0";
  }
  if (hours < 10) {
    hours_o = ":0";
  }
  return days + hours_o + hours + mins_o + mins + secs_o + secs;
}

long lastThMsg = 0;
long lastLdrMsg = 0;
long thPowerUp = 0;
long ldrPowerUp = 0;
bool dhtBegin = false;
float lastTemperature = 0.0f;
float lastHumidity = 0.0f;
int lastLightLevel = 0;


// **************************************************
// **********         Main Loop           ***********
// **************************************************
void loop() {
 snl = sizeof(SensorName)+1;
  long mNow = millis();
  //some DHT reads can take up to 2 seconds, so we check regulary
  processPIR();
  int mxt = maxMinutesBetweenSends * 60 * 1000;
  bool sendTemp = (mNow - lastSendTemperature >= mxt ? true : false);
  bool sendHum = (mNow - lastSendHumidity >= mxt ? true : false);
  if (lastThMsg == 0) {
    lastThMsg = mNow - THL_rate_limit;
  }
  if ((hasTemperature || hasHumidity) && ((mNow - lastThMsg) >= THL_rate_limit) || (sendTemp || sendHum)) {
    if (thPowerUp == 0) {
      digitalWrite(thPwr, HIGH);
      thPowerUp = millis();
    } else if (!dhtBegin && (mNow - thPowerUp) >= 100) {
      dht.begin();
      dhtBegin = true;
    } else if (dhtBegin && (mNow - thPowerUp) >= dhtCalibrationTimeMS) {

      char errorTopicSn[sizeof(errorTopic) + snl + 1];
      sprintf(errorTopicSn, errorTopic, SensorName);

      int chk = dht.read(thPin);

      Serial.print(F("Read sensor: "));
      switch (chk)
      {
        case 0: Serial.println(F("Possibly Bad")); sendMqtt(errorTopicSn, "Failed to read from DHT sensor! Error 0", true);  break; //possibly ok
        case -1: Serial.println(F("Checksum error")); sendMqtt(errorTopicSn, "Failed to read from DHT sensor! Checksum Error -1", true) ;  break;
        case -2: Serial.println(F("Time out error")); sendMqtt(errorTopicSn, "Failed to read from DHT sensor! Timeout Error -2", true);  break;
        case 1: Serial.println(F("OK")); break;
        default: Serial.print(F("Unknown error: ")); Serial.println(chk); sendMqtt(errorTopicSn, "Failed to read from DHT sensor! Unknown Error: ", true);   break;
      }
      processPIR();
      float vHumidity = (hasHumidity ? dht.readHumidity() : -1);
      processPIR();
      float vTemperature = (hasTemperature ? dht.readTemperature() : -1);

      if (hasHumidity) {
        if (isnan(vHumidity)) {
#if !sbDebug
          Serial.println(F("Failed to read Humidity"));
#endif
          lastHumidity = -99;
          sendMqtt(errorTopicSn, "Failed to read from Humidity sensor!", false);
        } else {
          if (fabs(vHumidity - lastHumidity) > 0.01 || sendHum) {
            lastHumidity = vHumidity;
            sendHumidity(vHumidity);
          }
        }
        processPIR();
      }
      if (hasTemperature) {
        if (isnan(vTemperature)) {
#if !sbDebug
          Serial.println(F("Failed to read Temperature"));
#endif
          lastTemperature = -99;
          sendMqtt(errorTopicSn, "Failed to read from Temperature sensor!", false);
        } else {
          if (fabs(vTemperature - lastTemperature) > 0.01 || sendTemp) {
            lastTemperature = vTemperature;
            sendTemperature(vTemperature);
          }
        }
        processPIR();
      }
      digitalWrite(thPwr, LOW);
      thPowerUp = 0;
      dhtBegin = false;
      lastThMsg = millis();
      //client.loop();
    }
  }
  if (hasLight && (mNow - lastLdrMsg) >= THL_rate_limit) {
    if (ldrPowerUp == 0) {
      digitalWrite(ldrPwr, HIGH);
      ldrPowerUp = millis();
    } else if ( (mNow - ldrPowerUp) >= ldrCalibrationTimeMS) {
      int ldr = analogRead(A0);
      if (ldr != lastLightLevel) {
        lastLightLevel = ldr;
        String ldrp_str = String(ldr);
        char *ldrc = new char[ldrp_str.length() + 1];
        strcpy(ldrc, ldrp_str.c_str());
        char ldrTopicSn[sizeof(ldrTopic) + snl + 1];
        sprintf(ldrTopicSn, ldrTopic, SensorName);
        sendMqtt(ldrTopicSn, ldrc, true);
      }
      digitalWrite(ldrPwr, LOW);
      ldrPowerUp = 0;
      lastLdrMsg = millis();
    }
  }
  processPIR();
  if (millis() - mNow > 300) {
    Serial.printf("\nTime Taken: %d ms\n\n", (millis() - mNow));
  }
}

#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

// Telnet server
WiFiServer telnetServer(23);
WiFiClient telnetClient;

// WiFiMulti
ESP8266WiFiMulti wifiMulti;

// IFTTT Credentials
String server = "http://maker.ifttt.com";
String eventName = "water_level_record";
String IFTTT_Key = "iH7pSxjtE5z_WnLhyg_qZ6G38R9X-Yr8B8Nxttcj8Z8";
String tempIFTTTUrl="https://maker.ifttt.com/trigger/water_level_record/with/key/iH7pSxjtE5z_WnLhyg_qZ6G38R9X-Yr8B8Nxttcj8Z8";

// IFTTT notify url
String url_email = "http://maker.ifttt.com/trigger/low_water_email/with/key/iH7pSxjtE5z_WnLhyg_qZ6G38R9X-Yr8B8Nxttcj8Z8";
String url_line = "http://maker.ifttt.com/trigger/low_water_level/with/key/iH7pSxjtE5z_WnLhyg_qZ6G38R9X-Yr8B8Nxttcj8Z8";

// NETPIE credentials
const char* mqtt_server = "broker.netpie.io";
const int mqtt_port = 1883;
const char* mqtt_Client = "xxxxx";
const char* mqtt_username = "xxxxx";
const char* mqtt_password = "xxxxx";

WiFiClient espClient;
PubSubClient client(espClient);

// Pins
#define trigPin 15
#define echoPin 13
#define pumpPin 2
 
long duration;
int distance;
int waterLevel;

String pumpControl = "off";
unsigned long lastNotifyTime = -21600000;
unsigned long notifyDelay = 21600000; // 6 hrs


void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting NETPIE2020 connection…");
    if (client.connect(mqtt_Client, mqtt_username, mqtt_password)) {
      Serial.println("NETPIE2020 connected");
      client.subscribe("@msg/pump");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  for (int i = 0; i < length; i++) {
    message = message + (char)payload[i];
  }
  Serial.println(message);
  if (String(topic) == "@msg/pump") {
    pumpControl = message;
  }
}

 
void setup() {
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, HIGH);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  Serial.begin(9600);
  Serial.println("Booting");
  delay(100);

  wifiMulti.addAP("xxxxx", "xxxxx");
  wifiMulti.addAP("xxxxx", "xxxxx");
  wifiMulti.addAP("xxxxx", "xxxxx");

  Serial.println("Connecting WiFi...");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WiFi connected");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  telnetServer.begin();
  telnetServer.setNoDelay(true);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("esp8266-03-water-level");

  // No authentication by default
  ArduinoOTA.setPassword("esp8266admin");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
 
void loop() {
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  ArduinoOTA.handle();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  water_level_measure();
  pump_control();
  sendDataToSheet();

  telnet_monitor();
  delay(1000);
}

void water_level_measure() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
 
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
 
  duration = pulseIn(echoPin, HIGH);
 
  distance = (duration/2) / 29.1;

  waterLevel = max(45 - distance, 0);

  if ((waterLevel >= 0) & (waterLevel < 5) & (millis() > lastNotifyTime + notifyDelay)) {
    WiFiClient emailClient;
    HTTPClient emailHttp;
    HTTPClient lineHttp;
    emailHttp.begin(emailClient, url_line);
    lineHttp.begin(emailClient, url_email);

    int emailHttpCode = emailHttp.GET();
    int lineHttpCode = lineHttp.GET();

    if (emailHttpCode > 0) {
      Serial.printf("Response Code: %d\n", emailHttpCode);

      if (emailHttpCode == HTTP_CODE_OK){
        String payload = emailHttp.getString();
        Serial.println(payload);
      }
    } 
    else {
      Serial.print("[HTTP] GET failed "); 
      Serial.print(emailHttpCode);
      Serial.println(emailHttp.errorToString(emailHttpCode).c_str());
    }
    emailHttp.end();

    if (lineHttpCode > 0) {
      Serial.printf("Response Code: %d\n", lineHttpCode);

      if (lineHttpCode == HTTP_CODE_OK){
        String payload = lineHttp.getString();
        Serial.println(payload);
      }
    } 
    else {
      Serial.print("[HTTP] GET failed "); 
      Serial.print(lineHttpCode);
      Serial.println(lineHttp.errorToString(lineHttpCode).c_str());
    }
    lineHttp.end();

    lastNotifyTime = millis();
  }

  Serial.print("Distance = ");
  Serial.print(distance);
  Serial.println(" cm");
}

void pump_control() {
  Serial.print("pumpControl = ");
  Serial.println(pumpControl);

  // Control the water pump
  if (pumpControl == "on") {
    digitalWrite(pumpPin, LOW);
    Serial.println("Pump On");
  } else {
    digitalWrite(pumpPin, HIGH);
    Serial.println("Pump Off");
  }
}

void sendDataToSheet()
{
  String url = server + "/trigger/" + eventName + "/with/key/" + IFTTT_Key + "?value1=" + String((int)waterLevel) + "&value2=&value3=";  
  Serial.println(url);
  //Start to send data to IFTTT
  WiFiClient sheetClient;
  HTTPClient sheetHttp;
  Serial.print("[HTTP] begin...\n");
  sheetHttp.begin(sheetClient, url); //HTTP

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int sheetHttpCode = sheetHttp.GET();
  // sheetHttpCode will be negative on error
  if(sheetHttpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", sheetHttpCode);
    // file found at server
    if(sheetHttpCode == HTTP_CODE_OK) {
      String payload = sheetHttp.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", sheetHttp.errorToString(sheetHttpCode).c_str());
  }
  sheetHttp.end();
}

void telnet_monitor() {
  if (telnetServer.hasClient()) {
    if (!telnetClient || !telnetClient.connected()) {
      if (telnetClient) {
        telnetClient.stop();
        Serial.println("Telnet Client Stop");
      }
      telnetClient = telnetServer.available();
      Serial.println("New Telnet client");
      telnetClient.flush();
    }
  }

  if (telnetClient && telnetClient.connected()) {
    if (isnan(duration) || isnan(distance) || isnan(waterLevel)) {
      telnetClient.println(F("Failed to read from ultrasonic sensor!"));
      return;
    }

    if (pumpControl == "on") {
      telnetClient.println("Pump On");
    }
    else {
      telnetClient.println("Pump Off");
    }

    telnetClient.print("Distance = ");
    telnetClient.print(distance);
    telnetClient.print(" cm");
    telnetClient.print(" , Water Level = ");
    telnetClient.print(waterLevel);
    telnetClient.println(" cm");
    telnetClient.println(pumpControl);
  }
}

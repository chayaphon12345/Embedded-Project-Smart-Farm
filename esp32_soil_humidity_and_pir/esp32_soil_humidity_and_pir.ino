#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

WiFiServer telnetServer(23);
WiFiClient telnetClient;

// WiFiMulti
WiFiMulti wifiMulti;

// NETPIE credentials
const char* mqtt_server = "broker.netpie.io";
const int mqtt_port = 1883;
const char* mqtt_Client = "xxxxx";
const char* mqtt_username = "xxxxx";
const char* mqtt_password = "xxxxx";

WiFiClient espClient;
PubSubClient client(espClient);

int soilMoisturePin = 32;   
int soilMoistureValue = 0;
bool pumpState = false;
int pirPin = 14;
int pirValue = LOW;
bool camState = false;

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting NETPIE2020 connection…");
    if (client.connect(mqtt_Client, mqtt_username, mqtt_password)) {
      Serial.println("NETPIE2020 connected");
    }
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  pinMode(soilMoisturePin, INPUT);
  pinMode(pirPin, INPUT);

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

  telnetServer.begin();
  telnetServer.setNoDelay(true);

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("esp32-01-soil-hygrometer");

  // No authentication by default
  ArduinoOTA.setPassword("esp32admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
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

  pir_sensor();
  soil_sensor();

  telnet_monitor();
  delay(1000);
}

void pir_sensor() {
  pirValue = digitalRead(pirPin);
  if (pirValue == HIGH) {
    if (!camState) {
      camState = true;
      client.publish("@msg/cam", "capture");
      Serial.println("Movement detect!");
    }
  }
  else {
    if (camState) {
      camState = false;
      client.publish("@msg/cam", "wait");
      Serial.println("Not detect any movement.");
    }
  }
}

void soil_sensor() {
  soilMoistureValue = analogRead(soilMoisturePin);
  Serial.print(soilMoistureValue);
  Serial.print(", ");
  soilMoistureValue = map(soilMoistureValue, 0, 4095, 100, 0);
  Serial.println(soilMoistureValue);

  if ((soilMoistureValue > 0) & (soilMoistureValue < 40)) {  // Adjust threshold value as needed
    if (pumpState) {
      Serial.println("Still too dry. Continue turn on a pump.");
    }
    else {
      pumpState = true;
      client.publish("@msg/pump", "on");
      Serial.println("Too dry. Turn on a pump.");
    }
    
  } else {
    if (pumpState) {
      pumpState = false;
      client.publish("@msg/pump", "off");
      Serial.println("Proper moisture. Turn off a pump.");
    }
    else {
      Serial.println("Still proper moisture. Continue turn off a pump.");
    }
  }
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
    if (isnan(soilMoistureValue)) {
      telnetClient.println(F("Failed to read from soil hygrometer sensor!"));
      return;
    }

    telnetClient.println("=======================");
    telnetClient.print("Pump State: ");
    telnetClient.print(pumpState);
    telnetClient.print(", Soil Humidity level = ");
    telnetClient.print(soilMoistureValue);
    telnetClient.println(" / 100");

    telnetClient.print("Cam State: ");
    telnetClient.print(camState);
    if (pirValue == HIGH) {
      telnetClient.println(", Movement detect!");
    }
    else {
      telnetClient.println(", Not detect any movement.");
    }
  }
}

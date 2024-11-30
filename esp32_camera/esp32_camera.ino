#include <WiFiMulti.h>
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
String camControl = "off";

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting NETPIE2020 connection…");
    if (client.connect(mqtt_Client, mqtt_username, mqtt_password)) {
      Serial.println("NETPIE2020 connected");
      client.subscribe("@msg/cam");
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
  if (String(topic) == "@msg/cam") {
    camControl = message;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

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

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("esp32-camera");

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

  telnet_monitor();
  delay(500);
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
    telnetClient.print("Camera capture status: ");
    telnetClient.println(camControl);
  }
}

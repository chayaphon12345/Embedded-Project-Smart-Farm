#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "DHT.h"

#define DHTPIN 2     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

DHT dht(DHTPIN, DHTTYPE);

WiFiServer telnetServer(23);
WiFiClient telnetClient;

// WiFiMulti
ESP8266WiFiMulti wifiMulti;

int lightPin = A0;   
int lightValue = 0;  

float h, t, f, hif, hic;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  wifiMulti.addAP("xxxxx", "xxxxx");
  wifiMulti.addAP("xxxxx", "xxxxx");

  Serial.println("Connecting WiFi...");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("WiFi connected");

  telnetServer.begin();
  telnetServer.setNoDelay(true);

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("esp8266-02-light-temperature");

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
  dht.begin();
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

  temperature_sensor();
  ldr_sensor();

  telnet_monitor();
  delay(10000);
}

void temperature_sensor() {
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));
}

void ldr_sensor() {
  lightValue = analogRead(lightPin);
  lightValue = map(lightValue, 0, 1024, 100, 0);
  Serial.print("Light Value = ");
  Serial.print(lightValue);
  Serial.println(" / 100");
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
    if (isnan(h) || isnan(t) || isnan(f)) {
      telnetClient.println(F("Failed to read from DHT sensor!"));
      return;
    }

    telnetClient.print(F("Humidity: "));
    telnetClient.print(h);
    telnetClient.print(F("%  Temperature: "));
    telnetClient.print(t);
    telnetClient.print(F("°C "));
    telnetClient.print(f);
    telnetClient.print(F("°F  Heat index: "));
    telnetClient.print(hic);
    telnetClient.print(F("°C "));
    telnetClient.print(hif);
    telnetClient.println(F("°F"));

    telnetClient.print("light value: ");
    telnetClient.println(lightValue);
  }
}
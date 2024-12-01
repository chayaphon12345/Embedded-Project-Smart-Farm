// #include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
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

// IFTTT Credentials
String server = "http://maker.ifttt.com";
String eventTempName = "temp_record";
String eventLightName = "light_record";
String IFTTT_Key = "iH7pSxjtE5z_WnLhyg_qZ6G38R9X-Yr8B8Nxttcj8Z8";
String tempIFTTTUrl="http://maker.ifttt.com/trigger/temp_record/with/key/iH7pSxjtE5z_WnLhyg_qZ6G38R9X-Yr8B8Nxttcj8Z8";
String lightIFTTTUrl="https://maker.ifttt.com/trigger/light_record/with/key/iH7pSxjtE5z_WnLhyg_qZ6G38R9X-Yr8B8Nxttcj8Z8";

int lightPin = A0;   
int lightValue = 0;  

float h, t, f, hif, hic;

long now = millis();
long lastMeasure = 0;

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

  // WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, password);
  // while (WiFi.waitForConnectResult() != WL_CONNECTED) {
  //   Serial.println("Connection Failed! Rebooting...");
  //   delay(5000);
  //   ESP.restart();
  // }

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

  now = millis();
  // Publishes new data to google sheet every 10 seconds //ส่งข้อมูลไปทุกๆ 10 วินาที (หน่วย ms)
  if (now - lastMeasure > 60000) {
    lastMeasure = now;
    
    temperature_sensor();
    ldr_sensor();
    sendDataToSheet();
  }

  telnet_monitor();
  delay(1000);
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

void sendDataToSheet()
{
  String url = server + "/trigger/" + eventTempName + "/with/key/" + IFTTT_Key + "?value1=" + String((float)t) + "&value2="+String((float)f) +"&value3=" + String((float)h);  
  Serial.println(url);
  //Start to send data to IFTTT
  WiFiClient client;
  HTTPClient http;
  Serial.print("[HTTP] begin...\n");
  http.begin(client, url); //HTTP

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
  // httpCode will be negative on error
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();

  String url2 = server + "/trigger/" + eventLightName + "/with/key/" + IFTTT_Key + "?value1=" + String((int)lightValue) + "&value2=&value3=";  
  Serial.println(url2);
  //Start to send data to IFTTT
  WiFiClient client2;
  HTTPClient http2;
  Serial.print("[HTTP] begin...\n");
  http2.begin(client2, url2); //HTTP

  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode2 = http2.GET();
  // httpCode will be negative on error
  if(httpCode2 > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode2);
    // file found at server
    if(httpCode2 == HTTP_CODE_OK) {
      String payload = http2.getString();
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http2.errorToString(httpCode2).c_str());
  }
  http2.end();
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

#include <M5Atom.h>
#include <WiFi.h>
#include <Arduino_JSON.h>
#include <PinButton.h>
#include <stdint.h>
#include <Arduino.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <Preferences.h>

// LED array pin (NeoPixelArray)
#define DATA_PIN_LED 27  

// ------- New definitions for external RGB LED -------
#define LEDC_CHANNEL_RED   0
#define LEDC_CHANNEL_GREEN 1
#define LEDC_CHANNEL_BLUE  2

const int LED_PIN_RED   = 33;  // External LED red channel
const int LED_PIN_GREEN = 22;  // External LED green channel
const int LED_PIN_BLUE  = 19;  // External LED blue channel
const int LED_PIN_GND   = 23;  // External LED ground pin

const int pwmFreq = 5000;      // 5 kHz PWM frequency
const int pwmResolution = 8;   // 8-bit resolution
// ------------------------------------------------------

/* M5 variables */
PinButton btnAction(39);  // the "Action" button on the device
Preferences preferences;

/* USER CONFIG VARIABLES */
// Companion Server
char companion_host[40] = "Companion IP";
char companion_port[6] = "16622";

// Set staticIP to 1 if you want a static IP address. Default is DHCP.
IPAddress stationIP = IPAddress(0, 0, 0, 0);
IPAddress stationGW = IPAddress(0, 0, 0, 0);
IPAddress stationMask = IPAddress(255, 255, 255, 0);

// Name of the device – the last 3 bytes of MAC will be appended for uniqueness.
String deviceID = "m5Atom-";

unsigned long lastTime = millis();
unsigned long currentTime;
const unsigned long timeout = 5000;

int brightness = 100;
uint8_t r = 0;
uint8_t g = 0;
uint8_t b = 0;

// M5Atom Access Point Password (min. 8 characters; leave empty for open AP)
const char* AP_password = "";

/* END OF USER VARIABLES */

WiFiManager WiFiManager;  // global WiFiManager instance
WiFiClient client;

// Logger - logs to serial
void logger(String strLog, String strType) {
  Serial.println(strLog);
}

String getParam(String name) {
  String value;
  if (WiFiManager.server->hasArg(name)) {
    value = WiFiManager.server->arg(name);
  }
  return value;
}

void saveParamCallback() {
  logger("[CALLBACK] saveParamCallback fired", "info-quiet");
  String str_companionIP = getParam("companionIP");
  String str_companionPort = getParam("companionPort");
  logger("Saving new Companion host", "info-quiet");
  preferences.begin("companion", false);
  preferences.putString("companionip", str_companionIP);
  preferences.putString("companionport", str_companionPort);
  logger("Prefs Companion Server = " + preferences.getString("companionip"), "info-quiet");
  logger("Prefs Companion Port = " + preferences.getString("companionport"), "info-quiet");
  preferences.end();
}

// WiFiManager Parameters
WiFiManagerParameter* custom_companionIP;
WiFiManagerParameter* custom_companionPort;

void connectToNetwork() {
  if (stationIP != IPAddress(0, 0, 0, 0)) {
    WiFiManager.setSTAStaticIPConfig(stationIP, stationGW, stationMask);
  }
  WiFi.mode(WIFI_STA);
  logger("Connecting to SSID: " + String(WiFi.SSID()), "info");

  custom_companionIP = new WiFiManagerParameter("companionIP", "Companion IP", companion_host, 40);
  custom_companionPort = new WiFiManagerParameter("companionPort", "Satellite Port", companion_port, 6);
  WiFiManager.addParameter(custom_companionIP);
  WiFiManager.addParameter(custom_companionPort);
  WiFiManager.setSaveParamsCallback(saveParamCallback);

  std::vector<const char*> menu = { "wifi", "param", "info", "sep", "restart", "exit" };
  WiFiManager.setMenu(menu);
  WiFiManager.setClass("invert");
  WiFiManager.setConfigPortalTimeout(120);

  bool res = WiFiManager.autoConnect(deviceID.c_str(), AP_password);
  if (!res) {
    logger("Failed to connect", "error");
    M5.dis.drawpix(0, 0xf00000);
    M5.dis.drawpix(4, 0xf00000);
    M5.dis.drawpix(20, 0xf00000);
    M5.dis.drawpix(24, 0xf00000);
  } else {
    logger("connected...yay :)", "info");
    M5.dis.drawpix(0, 0x0000f0);
    M5.dis.drawpix(4, 0x0000f0);
    M5.dis.drawpix(20, 0x0000f0);
    M5.dis.drawpix(24, 0x0000f0);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  logger("Initializing M5-Atom.", "info-quiet");

  // Save battery by turning off Bluetooth
  btStop();

  // Append last three bytes of MAC to deviceID for uniqueness
  byte mac[6];
  WiFi.macAddress(mac);
  deviceID = deviceID + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
  logger("DeviceID: " + deviceID, "info");

  // Load from non-volatile memory
  preferences.begin("companion", false);
  if (preferences.getString("deviceid").length() > 0) {
    deviceID = preferences.getString("deviceid");
  }
  if (preferences.getString("companionip").length() > 0) {
    String newHost = preferences.getString("companionip");
    logger("Setting Companion host as " + newHost, "info-quiet");
    newHost.toCharArray(companion_host, 40);
  }
  if (preferences.getString("companionport").length() > 0) {
    String newPort = preferences.getString("companionport");
    logger("Setting Companion port as " + newPort, "info-quiet");
    newPort.toCharArray(companion_port, 6);
  }
  preferences.end();

  // Set WiFi hostname
  WiFiManager.setHostname(deviceID.c_str());

  M5.begin(true, false, true);
  delay(50);
  M5.dis.drawpix(0, 0xf00000);

  // --------- Initialize external RGB LED PWM ---------
  ledcSetup(LEDC_CHANNEL_RED, pwmFreq, pwmResolution);
  ledcSetup(LEDC_CHANNEL_GREEN, pwmFreq, pwmResolution);
  ledcSetup(LEDC_CHANNEL_BLUE, pwmFreq, pwmResolution);
  ledcAttachPin(LED_PIN_RED, LEDC_CHANNEL_RED);
  ledcAttachPin(LED_PIN_GREEN, LEDC_CHANNEL_GREEN);
  ledcAttachPin(LED_PIN_BLUE, LEDC_CHANNEL_BLUE);
  
  // Set the external LED ground pin as OUTPUT and hold it LOW
  pinMode(LED_PIN_GND, OUTPUT);
  digitalWrite(LED_PIN_GND, LOW);
  // -----------------------------------------------------

  connectToNetwork();

  // Debug info
  char message[200];
  sprintf(message, "After preferences.end Companion Host is: %s Satellite Port is: %s", companion_host, companion_port);
  logger(message, "info-quiet");

  ArduinoOTA.setHostname(deviceID.c_str());
  ArduinoOTA.setPassword("companion-satellite");
  ArduinoOTA.onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else
        type = "filesystem";
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
      if (error == OTA_AUTH_ERROR) logger("Auth Failed", "error");
      else if (error == OTA_BEGIN_ERROR) logger("Begin Failed", "error");
      else if (error == OTA_CONNECT_ERROR) logger("Connect Failed", "error");
      else if (error == OTA_RECEIVE_ERROR) logger("Receive Failed", "error");
      else if (error == OTA_END_ERROR) logger("End Failed", "error");
    });

  ArduinoOTA.begin();
  delay(100);
}

void loop() {
  M5.dis.fillpix(0x000000);
  M5.dis.drawpix(0, 0x0000f0);
  M5.dis.drawpix(4, 0x0000f0);
  M5.dis.drawpix(20, 0x0000f0);
  M5.dis.drawpix(24, 0x0000f0);

  if (M5.Btn.wasPressed()) {
    M5.dis.fillpix(0xffffff);
    logger("---------------------------------", "info-quiet");
    logger("Button Pressed.", "info-quiet");
    logger("M5Atom IP Address: " + WiFi.localIP().toString(), "info-quiet");
    logger("Companion Server: " + String(companion_host), "info-quiet");
    logger("Companion Port: " + String(companion_port), "info-quiet");
    logger("Device ID: " + String(deviceID), "info-quiet");
    logger("---------------------------------", "info-quiet");
  }

  if (M5.Btn.wasReleased()) {
    M5.dis.fillpix(0x000000);
    logger("---------------------------------", "info-quiet");
    logger("Button Released.", "info-quiet");
    logger("M5Atom IP Address: " + WiFi.localIP().toString(), "info-quiet");
    logger("Companion Server: " + String(companion_host), "info-quiet");
    logger("Companion Port: " + String(companion_port), "info-quiet");
    logger("Device ID: " + String(deviceID), "info-quiet");
    logger("---------------------------------", "info-quiet");
  }

  if (M5.Btn.pressedFor(5000)) {
    M5.dis.fillpix(0x000000);
    M5.dis.drawpix(0, 0xf000f0);
    M5.dis.drawpix(4, 0xf000f0);
    M5.dis.drawpix(20, 0xf000f0);
    M5.dis.drawpix(24, 0xf000f0);
    while (WiFiManager.startConfigPortal(deviceID.c_str(), AP_password)) {}
    ESP.restart();
  }

  delay(100);
  M5.update();

  String apiPort = String(companion_port);
  currentTime = millis();
  if ((currentTime - lastTime >= timeout) && (client.connect(companion_host, apiPort.toInt()))) {
    logger("Connected to API", "info");

    M5.dis.drawpix(0, 0x00f000);
    M5.dis.drawpix(4, 0x00f000);
    M5.dis.drawpix(20, 0x00f000);
    M5.dis.drawpix(24, 0x00f000);

    client.println("add-device DEVICEID=" + deviceID + " PRODUCT_NAME=\"M5 Atom Matrix\" \\n COLORS=rgb KEYS_TOTAL=1 KEYS_PER_ROW=1 TEXT=true");

    while (client.connected()) {
      while (client.available()) {
        String apiData = client.readStringUntil('\n');
        parseAPI(apiData);
      }

      if (M5.Btn.wasPressed()) {
        client.println("KEY-PRESS DEVICEID=" + deviceID + " KEY=0 PRESSED=true");
        logger("---------------------------------", "info-quiet");
        logger("Button Pressed.", "info-quiet");
        logger("Device ID: " + String(deviceID), "info-quiet");
        logger("---------------------------------", "info-quiet");
      }

      if (M5.Btn.wasReleased()) {
        client.println("KEY-PRESS DEVICEID=" + deviceID + " KEY=0 PRESSED=false");
        logger("---------------------------------", "info-quiet");
        logger("Button Released.", "info-quiet");
        logger("Device ID: " + String(deviceID), "info-quiet");
        logger("---------------------------------", "info-quiet");
      }

      currentTime = millis();
      if (currentTime - lastTime >= 2000) {
        client.println("ping");
        lastTime = currentTime;
      }

      M5.update();
      delay(100);
    }
  }
}

void parseAPI(String apiData) {
  if (apiData.startsWith("PONG")) {
    return;
  }
  logger(apiData, "API");

  if (apiData.startsWith("PING")) {
    String payload = apiData.substring(apiData.indexOf(" ") + 1);
    logger("Returning Payload: " + payload, "info-quiet");
    client.println(payload);
  }

  if (apiData.startsWith("BRIGHTNESS")) {
    String value = apiData.substring(apiData.indexOf("VALUE=") + 6);
    brightness = value.toInt();
    logger("Brightness Set: " + value, "info-quiet");
    setColor(r, g, b);
  }

  if (apiData.startsWith("KEYS-CLEAR")) {
    setColor(0, 0, 0);
  }

  if (apiData.startsWith("KEY-STATE")) {
    int colorPos = apiData.indexOf("COLOR=");
    int rEnd = apiData.indexOf(",", colorPos);
    int gEnd = apiData.indexOf(",", rEnd + 1);
    int bEnd = apiData.indexOf(",", gEnd + 1);

    String strr = apiData.substring(colorPos + 11, rEnd);
    String strg = apiData.substring(rEnd + 1, gEnd);
    String strb = apiData.substring(gEnd + 1, bEnd);

    r = strr.toInt();
    g = strg.toInt();
    b = strb.toInt();

    logger("r:" + String(r) + " g:" + String(g) + " b:" + String(b), "info-quiet");
    setColor(r, g, b);
  }
}

// Modified setColor now updates both the display and the external RGB LED via PWM.
void setColor(uint8_t newR, uint8_t newG, uint8_t newB) {
  uint8_t scaledR = newR * max(brightness, 15) / 100;
  uint8_t scaledG = newG * max(brightness, 15) / 100;
  uint8_t scaledB = newB * max(brightness, 15) / 100;

  int rgb = (scaledR << 16) | (scaledG << 8) | scaledB;
  M5.dis.fillpix(rgb);

  ledcWrite(LEDC_CHANNEL_RED, scaledR);
  ledcWrite(LEDC_CHANNEL_GREEN, scaledG);
  ledcWrite(LEDC_CHANNEL_BLUE, scaledB);
}

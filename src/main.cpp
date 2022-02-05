#include <credentials.h>
#include <EspMQTTClient.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>
#include <MqttKalmanPublish.h>

#define CLIENT_NAME "espMatrix"
const bool MQTT_RETAINED = true;

EspMQTTClient client(
    WIFI_SSID,
    WIFI_PASSWORD,
    MQTT_SERVER,
    MQTT_USERNAME, // Can be omitted if not needed
    MQTT_PASSWORD, // Can be omitted if not needed
    CLIENT_NAME,   // Client name that uniquely identify your device
    1883           // The MQTT port, default to 1883. this line can be omitted
);

#define BASE_TOPIC CLIENT_NAME "/"
#define BASE_TOPIC_SET BASE_TOPIC "set/"
#define BASE_TOPIC_STATUS BASE_TOPIC "status/"

const uint16_t WIDTH = 32;
const uint16_t HEIGHT = 8;

const int PIN_MATRIX = 13; // D7
const int PIN_ON = 5;      // D1

CRGB leds[WIDTH * HEIGHT];

//   NEO_MATRIX_TOP, NEO_MATRIX_BOTTOM, NEO_MATRIX_LEFT, NEO_MATRIX_RIGHT:
//     Position of the FIRST LED in the matrix; pick two, e.g.
//     NEO_MATRIX_TOP + NEO_MATRIX_LEFT for the top-left corner.
//   NEO_MATRIX_ROWS, NEO_MATRIX_COLUMNS: LEDs are arranged in horizontal
//     rows or in vertical columns, respectively; pick one or the other.
//   NEO_MATRIX_PROGRESSIVE, NEO_MATRIX_ZIGZAG: all rows/columns proceed
//     in the same order, or alternate lines reverse direction; pick one.
FastLED_NeoMatrix matrix = FastLED_NeoMatrix(leds, WIDTH, HEIGHT,
  NEO_MATRIX_BOTTOM  + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);

MQTTKalmanPublish mkRssi(client, BASE_TOPIC_STATUS "rssi", MQTT_RETAINED, 12 * 5 /* every 5 min */, 10);

String text = "hey!";
uint16_t hue = 120; // green
uint8_t sat = 100;
uint8_t bri = 10;
boolean on = true;

uint8_t hue8 = hue / 360.0 * 256.0;
uint8_t sat8 = sat / 100.0 * 255.0;
int x = 0;
int textPixelWidth = text.length() * 6;
boolean textIsLongerThanMatrix = textPixelWidth > WIDTH;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_ON, OUTPUT);
  Serial.begin(115200);

  FastLED.addLeds<NEOPIXEL, PIN_MATRIX>(leds, WIDTH * HEIGHT);
  matrix.begin();
  matrix.setBrightness(bri * on);
  matrix.setCursor(0, 0);
  auto color = CRGB(CHSV(hue8, sat8, 255));
  matrix.setTextColor(matrix.Color(color.red, color.green, color.blue));
  matrix.setTextWrap(false);
  matrix.print(text);

  client.enableDebuggingMessages();
  client.enableHTTPWebUpdater();
  client.enableOTA();
  client.enableLastWillMessage(BASE_TOPIC "connected", "0", MQTT_RETAINED);
}

void onConnectionEstablished() {
  client.subscribe(BASE_TOPIC_SET "text", [](const String &payload) {
    if (text != payload) {
      text = payload;
      textPixelWidth = text.length() * 6;
      textIsLongerThanMatrix = textPixelWidth > WIDTH;
      x = textIsLongerThanMatrix ? WIDTH : 0;
      client.publish(BASE_TOPIC_STATUS "text", String(text), MQTT_RETAINED);
    }
  });

  client.subscribe(BASE_TOPIC_SET "hue", [](const String &payload) {
    int parsed = strtol(payload.c_str(), 0, 10);
    uint16_t newValue = parsed % 360;
    if (hue != newValue) {
      hue = newValue;
      hue8 = hue / 360.0 * 256.0;
      auto color = CRGB(CHSV(hue8, sat8, 255));
      matrix.setTextColor(matrix.Color(color.red, color.green, color.blue));
      client.publish(BASE_TOPIC_STATUS "hue", String(hue), MQTT_RETAINED);
    }
  });

  client.subscribe(BASE_TOPIC_SET "sat", [](const String &payload) {
    int parsed = strtol(payload.c_str(), 0, 10);
    uint8_t newValue = max(0, min(100, parsed));
    if (sat != newValue) {
      sat = newValue;
      sat8 = sat / 100.0 * 255.0;
      auto color = CRGB(CHSV(hue8, sat8, 255));
      matrix.setTextColor(matrix.Color(color.red, color.green, color.blue));
      client.publish(BASE_TOPIC_STATUS "sat", String(sat), MQTT_RETAINED);
    }
  });

  client.subscribe(BASE_TOPIC_SET "bri", [](const String &payload) {
    int parsed = strtol(payload.c_str(), 0, 10);
    uint8_t newValue = max(0, min(255, parsed));
    if (bri != newValue) {
      bri = newValue;
      matrix.setBrightness(bri * on);
      client.publish(BASE_TOPIC_STATUS "bri", String(bri), MQTT_RETAINED);
    }
  });

  client.subscribe(BASE_TOPIC_SET "on", [](const String &payload) {
    boolean newValue = payload != "0";
    if (on != newValue) {
      on = newValue;
      matrix.setBrightness(bri * on);
      client.publish(BASE_TOPIC_STATUS "on", String(on), MQTT_RETAINED);
    }
  });

  // client.publish(BASE_TOPIC_STATUS "text", text, MQTT_RETAINED);
  client.publish(BASE_TOPIC_STATUS "hue", String(hue), MQTT_RETAINED);
  client.publish(BASE_TOPIC_STATUS "sat", String(sat), MQTT_RETAINED);
  client.publish(BASE_TOPIC_STATUS "bri", String(bri), MQTT_RETAINED);
  client.publish(BASE_TOPIC_STATUS "on", on ? "1" : "0", MQTT_RETAINED);
  client.publish(BASE_TOPIC "git-version", GIT_VERSION, MQTT_RETAINED);
  client.publish(BASE_TOPIC "connected", "2", MQTT_RETAINED);
}

void loop() {
  client.loop();
  digitalWrite(LED_BUILTIN, client.isConnected() ? HIGH : LOW);
  if (!client.isConnected()) {
    // Keep content until it is connected and updates again
    return;
  }

  digitalWrite(PIN_ON, on ? HIGH : LOW);
  matrix.fillScreen(0);
  matrix.setCursor(x, 0);
  matrix.print(text);

  auto now = millis();

  static unsigned long nextMove = 0;
  if (textIsLongerThanMatrix && now >= nextMove) {
    nextMove = now + 40;

    x -= 1;
    if (x < -textPixelWidth) {
      x = matrix.width();
    }
  }

  static unsigned long nextUpdate = 0;
  if (now >= nextUpdate) {
    nextUpdate = now + 20;
    matrix.show();
  }

  static unsigned long nextMeasure = 0;
  if (now >= nextMeasure) {
    nextMeasure = now + 5000;
    long rssi = WiFi.RSSI();
    float avgRssi = mkRssi.addMeasurement(rssi);
    Serial.printf("RSSI        in     dBm: %3ld   Average: %6.2f\n", rssi, avgRssi);
  }
}

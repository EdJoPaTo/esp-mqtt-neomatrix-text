#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <EspMQTTClient.h>

#define CLIENT_NAME "espMatrix"

EspMQTTClient client(
  "WifiSSID",
  "WifiPassword",
  "192.168.1.100",  // MQTT Broker server ip
  "MQTTUsername",   // Can be omitted if not needed
  "MQTTPassword",   // Can be omitted if not needed
  CLIENT_NAME, // Client name that uniquely identify your device
  1883 // The MQTT port, default to 1883. this line can be omitted
);

const bool mqtt_retained = true;

#define BASIC_TOPIC CLIENT_NAME "/"
#define BASIC_TOPIC_SET BASIC_TOPIC "set/"
#define BASIC_TOPIC_STATUS BASIC_TOPIC "status/"

const int PIN_MATRIX = 13; // D7
const int PIN_ON = 5; // D1

// MATRIX DECLARATION:
// Parameter 1 = width of NeoPixel matrix
// Parameter 2 = height of matrix
// Parameter 3 = pin number (most are valid)
// Parameter 4 = matrix layout flags, add together as needed:
//   NEO_MATRIX_TOP, NEO_MATRIX_BOTTOM, NEO_MATRIX_LEFT, NEO_MATRIX_RIGHT:
//     Position of the FIRST LED in the matrix; pick two, e.g.
//     NEO_MATRIX_TOP + NEO_MATRIX_LEFT for the top-left corner.
//   NEO_MATRIX_ROWS, NEO_MATRIX_COLUMNS: LEDs are arranged in horizontal
//     rows or in vertical columns, respectively; pick one or the other.
//   NEO_MATRIX_PROGRESSIVE, NEO_MATRIX_ZIGZAG: all rows/columns proceed
//     in the same order, or alternate lines reverse direction; pick one.
//   See example below for these values in action.
// Parameter 5 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)


// Example for NeoPixel Shield.  In this application we'd like to use it
// as a 5x8 tall matrix, with the USB port positioned at the top of the
// Arduino.  When held that way, the first pixel is at the top right, and
// lines are arranged in columns, progressive order.  The shield uses
// 800 KHz (v2) pixels that expect GRB color data.
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, PIN_MATRIX,
  NEO_MATRIX_BOTTOM     + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

String text = "hey!";
uint16_t hue = 120; // green
uint8_t sat = 100;
uint8_t bri = 10;
boolean on = true;
int x = 0;

int textPixelWidth() {
  return text.length() * 6;
}

bool isTextLongerThanMatrix() {
  return textPixelWidth() > matrix.width();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  pinMode(PIN_ON, OUTPUT);
  Serial.begin(115200);
  matrix.begin();
  matrix.setTextWrap(false);
  //matrix.setBrightness(50);

  matrix.setCursor(0, 0);
  matrix.setTextColor(matrix.ColorHSV(hue * 182, sat * 2.55, bri));
  matrix.print(text);
  matrix.show();

  // Optional functionnalities of EspMQTTClient
  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableLastWillMessage(BASIC_TOPIC "connected", "0", mqtt_retained);  // You can activate the retain flag by setting the third parameter to true
}

void onConnectionEstablished() {
  client.subscribe(BASIC_TOPIC_SET "hue", [](const String & payload) {
    int newHue = strtol(payload.c_str(), 0, 10);
    if (hue != newHue) {
      hue = newHue;
      client.publish(BASIC_TOPIC_STATUS "hue", payload, mqtt_retained);
    }
  });

  client.subscribe(BASIC_TOPIC_SET "sat", [](const String & payload) {
    int newSat = strtol(payload.c_str(), 0, 10);
    if (sat != newSat) {
      sat = newSat;
      client.publish(BASIC_TOPIC_STATUS "sat", payload, mqtt_retained);
    }
  });

  client.subscribe(BASIC_TOPIC_SET "bri", [](const String & payload) {
    int newBri = strtol(payload.c_str(), 0, 10);
    if (bri != newBri) {
      bri = newBri;
      client.publish(BASIC_TOPIC_STATUS "bri", payload, mqtt_retained);
    }
  });

  client.subscribe(BASIC_TOPIC_SET "on", [](const String & payload) {
    boolean newOn = payload != "0";
    if (on != newOn) {
      on = newOn;
      client.publish(BASIC_TOPIC_STATUS "on", payload, mqtt_retained);
    }
  });

  client.subscribe(BASIC_TOPIC_SET "text", [](const String & payload) {
    if (text != payload) {
      text = payload;
      x = isTextLongerThanMatrix() ? matrix.width() : 0;
      client.publish(BASIC_TOPIC_STATUS "text", payload, mqtt_retained);
    }
  });

  client.publish(BASIC_TOPIC "connected", "2", mqtt_retained);
  client.publish(BASIC_TOPIC_STATUS "hue", String(hue), mqtt_retained);
  client.publish(BASIC_TOPIC_STATUS "sat", String(sat), mqtt_retained);
  client.publish(BASIC_TOPIC_STATUS "bri", String(bri), mqtt_retained);
  client.publish(BASIC_TOPIC_STATUS "on", on ? "1" : "0", mqtt_retained);
  // client.publish(BASIC_TOPIC_STATUS "text", text, mqtt_retained);

  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  client.loop();

  digitalWrite(PIN_ON, on ? HIGH : LOW);

  matrix.fillScreen(0);
  matrix.setCursor(x, 0);
  matrix.print(text);
  matrix.setTextColor(matrix.ColorHSV(hue * 182, sat * 2.55, bri * on));
  matrix.show();

  if (isTextLongerThanMatrix()) {
    x -= 1;
    if (x < -textPixelWidth()) {
      x = matrix.width();
    }
  } else {
    x = 0;
  }

  delay(50);
}

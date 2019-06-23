#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>

// Update these with values suitable for your network.

const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
const bool mqtt_retained = true;

#define CLIENT_NAME "espMatrix"

#define BASIC_TOPIC CLIENT_NAME "/"
#define BASIC_TOPIC_SET BASIC_TOPIC "set/"
#define BASIC_TOPIC_STATUS BASIC_TOPIC "status/"

const int PIN = 13;

WiFiClient espClient;
PubSubClient client(espClient);

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
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 8, PIN,
  NEO_MATRIX_BOTTOM     + NEO_MATRIX_RIGHT +
  NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB            + NEO_KHZ800);

String text = "hey!";
uint16_t hue = 21845; // green
uint8_t sat = 255;
uint8_t bri = 25;
boolean on = true;
int x = 0;

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

int textPixelWidth() {
  return text.length() * 6;
}

bool isTextLongerThanMatrix() {
  return textPixelWidth() > matrix.width();
}

void setText(String newText) {
  if (text == newText) {
    return;
  }

  text = newText;
  x = isTextLongerThanMatrix() ? matrix.width() : 0;
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String payloadString = "";
  for (unsigned int i = 0; i < length; i++) {
    payloadString += (char)payload[i];
  }
  Serial.print(payloadString.length());
  Serial.print(" ");
  Serial.print(payloadString);

  if (strcmp(topic, BASIC_TOPIC_SET "hue") == 0) {
    hue = strtol(payloadString.c_str(), 0, 10) * 182; // move 360 to 2^16
    client.publish(BASIC_TOPIC_STATUS "hue", payloadString.c_str(), mqtt_retained);
  } else if (strcmp(topic, BASIC_TOPIC_SET "sat") == 0) {
    sat = strtol(payloadString.c_str(), 0, 10) * 2.55;
    client.publish(BASIC_TOPIC_STATUS "sat", payloadString.c_str(), mqtt_retained);
  } else if (strcmp(topic, BASIC_TOPIC_SET "bri") == 0) {
    bri = strtol(payloadString.c_str(), 0, 10);
    client.publish(BASIC_TOPIC_STATUS "bri", payloadString.c_str(), mqtt_retained);
  } else if (strcmp(topic, BASIC_TOPIC_SET "on") == 0) {
    on = payloadString != "0";
    client.publish(BASIC_TOPIC_STATUS "on", payloadString.c_str(), mqtt_retained);
  } else {
    setText(payloadString);
    client.publish(BASIC_TOPIC_STATUS "text", payloadString.c_str(), mqtt_retained);
  }

  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(CLIENT_NAME, BASIC_TOPIC "connected", 0, mqtt_retained, "0")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(BASIC_TOPIC "connected", "2", mqtt_retained);
      // ... and resubscribe
      client.subscribe(BASIC_TOPIC_SET "hue");
      client.subscribe(BASIC_TOPIC_SET "sat");
      client.subscribe(BASIC_TOPIC_SET "bri");
      client.subscribe(BASIC_TOPIC_SET "on");
      client.subscribe(BASIC_TOPIC_SET "text");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  Serial.begin(115200);
  matrix.begin();
  matrix.setTextWrap(false);
  //matrix.setBrightness(50);

  matrix.setCursor(0, 0);
  matrix.setTextColor(matrix.ColorHSV(hue, sat, bri));
  matrix.print(text);
  matrix.show();

  WiFi.hostname(CLIENT_NAME);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    digitalWrite(LED_BUILTIN, LOW);
    reconnect();
    digitalWrite(LED_BUILTIN, HIGH);
  }
  client.loop();

  matrix.fillScreen(0);
  matrix.setCursor(x, 0);
  matrix.print(text);
  matrix.setTextColor(matrix.ColorHSV(hue, sat, bri * on));
  matrix.show();

  if (isTextLongerThanMatrix() || x > 0) {
    x -= 1;
  }
  if (x < -textPixelWidth()) {
    x = matrix.width();
  }
  delay(50);
}

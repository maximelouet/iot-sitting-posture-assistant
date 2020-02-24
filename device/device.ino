#include "esp_wpa2.h"
#include "secrets.h"
#include <WiFi.h>
#include <PubSubClient.h>

#define SHORT_BEEP 1
#define MEDIUM_BEEP 2
#define LONG_BEEP 3
#define TWO_SHORT_BEEPS 4
#define TWO_LONG_BEEPS 5
#define THREE_SHORT_BEEPS 6
#define THREE_LONG_BEEPS 6

// PINS
const int beepbeep = 21; // i'm a sheep
const int echoOne = 26;
const int trigOne = 25;
const int echoTwo = 27;
const int trigTwo = 33;
const int echoThree = 36;
const int trigThree = 4;

// SETTINGS
const int acceptable_min = 4; // maximum distance in cm to be considered in range
const int in_range_up_to = 25; // distance in cm from which alerts are disabled
const int acceptable_bad_streak = 5; // maximum consecutive bad positions before beeping
const char *mqtt_server = "iot.saumon.io";
const int mqtt_port = 1883;
const char *ssid = "eduroam"; // WiFi SSID


WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
int bad_streak = 0;
bool mqtt_published = false;
bool alerting_position = false;


void beep(int pattern) {
  digitalWrite(beepbeep, HIGH);
  if (pattern == SHORT_BEEP) {
    delay(80);
  } else if (pattern == MEDIUM_BEEP) {
    delay(150);
  } else if (pattern == LONG_BEEP) {
    delay(350);
  } else if (pattern == TWO_SHORT_BEEPS) {
    delay(80);
    digitalWrite(beepbeep, LOW);
    delay(150);
    digitalWrite(beepbeep, HIGH);
    delay(80);
  }
  digitalWrite(beepbeep, LOW);
}

void mqtt_reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Connecting to MQTT broker...");
    String clientId = "saumon-iot-esp32";
    // Attempt to connect
    if (mqtt_client.connect(clientId.c_str())) {
      Serial.println(" connected");
      beep(TWO_SHORT_BEEPS);
    } else {
      Serial.print(" FAILED, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(", retrying in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      beep(LONG_BEEP);
    }
  }
}


void setup() {
  pinMode(beepbeep, OUTPUT);
  pinMode(echoOne, INPUT);
  pinMode(trigOne, OUTPUT);
  pinMode(echoTwo, INPUT);
  pinMode(trigTwo, OUTPUT);
  pinMode(echoThree, INPUT);
  pinMode(trigThree, OUTPUT);
  Serial.begin(115200);

  delay(100);

  //// WIFI CONNECTION

  byte error = 0;

  Serial.print("Connecting to ");
  Serial.print(ssid);
  Serial.println("...");

  WiFi.disconnect(true); // disconnect from wifi to set new wifi connection
  WiFi.mode(WIFI_STA);
  error += esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
  error += esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_IDENTITY, strlen(EAP_IDENTITY));
  error += esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));
  if (error != 0) {
    Serial.println("Error setting WPA properties.");
  }
  WiFi.enableSTA(true);

  esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
  if (esp_wifi_sta_wpa2_ent_enable(&config) != ESP_OK) {
    Serial.println("WPA2 Settings Not OK");
  }

  WiFi.begin(ssid); // connect to network
  WiFi.setHostname("poissonnerie-esp32"); //set Hostname for your device - not necessary
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected! My IP is ");
  Serial.println(WiFi.localIP());
  beep(TWO_SHORT_BEEPS);

  //// END WIFI CONNECTION

  //// MQTT

  mqtt_client.setServer(mqtt_server, mqtt_port);

  //// END MQTT
}

void loop() {
  bool good_position = true;

  //// MQTT

  if (!mqtt_client.connected()) {
    mqtt_reconnect();
  }

  //// END MQTT


  //// GET DISTANCES
  
  digitalWrite(trigOne, LOW);
  digitalWrite(trigTwo, LOW);
  digitalWrite(trigThree, LOW);
  delayMicroseconds(2);

  // trigger ultrasonic sensor 1
  digitalWrite(trigOne, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigOne, LOW);

  // read travel time 1
  long durationOne = pulseIn(echoOne, HIGH);

  // trigger ultrasonic sensor 2
  digitalWrite(trigTwo, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigTwo, LOW);

  // read travel time 2
  long durationTwo = pulseIn(echoTwo, HIGH);

  // trigger ultrasonic sensor 3
  digitalWrite(trigThree, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigThree, LOW);

  // read travel time 3
  long durationThree = pulseIn(echoThree, HIGH);

  // calculate distances in cm
  int distanceOne = durationOne * 0.034/2;
  int distanceTwo = durationTwo * 0.034/2;
  int distanceThree = durationThree * 0.034/2;

  //// END GET DISTANCES


  //// MAIN DISTANCE AND ALERT LOGIC

  bool in_range = distanceOne <= in_range_up_to && distanceTwo <= in_range_up_to && distanceThree <= in_range_up_to;
  if (in_range) {
    good_position = distanceOne <= acceptable_min && distanceTwo <= acceptable_min && distanceThree <= acceptable_min;
    if (good_position) {
      bad_streak = 0;
      if (alerting_position || !mqtt_published) {
        mqtt_client.publish("data/position", "good");
        mqtt_published = true;
      }
      alerting_position = false;
    } else {
      bad_streak++;
      if (bad_streak > acceptable_bad_streak) {
        if (!alerting_position) {
          mqtt_client.publish("data/position", "bad");
        }
        alerting_position = true;
        beep(SHORT_BEEP);
      }
    }
  } else {
    bad_streak = 0;
    alerting_position = false;
    mqtt_published = false;
  }

  //// END MAIN DISTANCE AND ALERT LOGIC


  //// DEBUG PRINTING

  Serial.print("Distance one ");
  Serial.print(distanceOne);
  Serial.print(", two ");
  Serial.print(distanceTwo);
  Serial.print(", three ");
  Serial.print(distanceThree);
  if (in_range) {
    Serial.print("; good position: ");
    Serial.print(good_position ? "yes" : "no");
    Serial.print("; alerting: ");
    Serial.println(alerting_position ? "yes" : "no");
  } else {
    Serial.println("; out of range");
    delay(600);
  }

  //// END DEBUG PRINTING

  delay(400);
}

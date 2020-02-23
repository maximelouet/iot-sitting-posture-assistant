#include "esp_wpa2.h"
#include "secrets.h"
#include <WiFi.h>
#include <PubSubClient.h>

#define MQTT_KEEPALIVE 10 // change default 15 seconds keepalive to match Mosquitto's 10 seconds

// PINS
const int beepbeep = 26; // i'm a sheep
const int echoOne = 12;
const int trigOne = 13;
const int echoTwo = 33;
const int trigTwo = 27;

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


void ringBeep() {
  digitalWrite(beepbeep, HIGH);
  delay(80);
  digitalWrite(beepbeep, LOW);
}

void mqtt_reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "saumon-esp32-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqtt_client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
  pinMode(beepbeep, OUTPUT);
  pinMode(echoOne, INPUT);
  pinMode(trigOne, OUTPUT);
  pinMode(echoTwo, INPUT);
  pinMode(trigTwo, OUTPUT);
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
  
  // calculate distance in cm
  int distanceOne = durationOne * 0.034/2;
  int distanceTwo = durationTwo * 0.034/2;

  //// END GET DISTANCES


  //// MAIN DISTANCE AND ALERT LOGIC

  bool in_range = distanceOne <= in_range_up_to && distanceTwo <= in_range_up_to;
  if (in_range) {
    good_position = distanceOne <= acceptable_min && distanceTwo <= acceptable_min;
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
        ringBeep();
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

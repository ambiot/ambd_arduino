/*
  Basic MQTT example

  This sketch demonstrates the basic capabilities of the library.
  It connects to an MQTT server then:
  - publishes "hello world" to the topic "outTopic"
  - subscribes to the topic "inTopic", printing out any messages
    it receives. NB - it assumes the received payloads are strings not binary

  It will reconnect to the server if the connection is lost using a blocking
  reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
  achieve the same result without blocking the main loop.

*/

#define USE_ESP32_AT      true
#include <ESP8266_AT_WebServer.h>
#include <PubSubClient.h>

char ssid[] = "xiaomi_test";        // your network SSID (name)
char pass[] = "1234567890";        // your network password
int status = WL_IDLE_STATUS;

char mqttServer[]     = "test.mosquitto.org";
char clientId[]       = "amebaClient";
char publishTopic[]   = "outTopic/amebad/test";
char publishPayload[] = "hello world, testing using DUE + ATCMD";
char subscribeTopic[] = "inTopic/amebad/test";

ESP8266_AT_Client atClient;
PubSubClient client(atClient);

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientId)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(publishTopic, publishPayload);
      // ... and resubscribe
      client.subscribe(subscribeTopic);
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
  Serial.begin(115200);
  Serial1.begin(115200);

  // initialize ESP module
  WiFi.init(&Serial1);
  Serial.println(F("WiFi shield init done"));

  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println(F("WiFi shield not present"));
    // don't continue
    while (true);
  }

  // attempt to connect to WiFi network
  while (status != WL_CONNECTED) {
    Serial.print(F("Connecting to SSID: "));
    Serial.println(ssid);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(ssid, pass);
  }

  client.setServer(mqttServer, 1883);
  client.setCallback(callback);

  delay(5000);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(500);
}

#include <Bounce2.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>

#include "wifi_credentials.h"
/* contains:
  const char* wifi_ssid = "wifissid";
  const char* wifi_password = "wifipassword";
*/
#define BUFFER_LENGTH 100

char mqtt_broker[BUFFER_LENGTH] = "";
char mqtt_topic[BUFFER_LENGTH] = "";
int mqtt_broker_port = 0;

unsigned long previous_millis = 0;

const int relay_pin = D2;
const int button_pin = D5;
bool relay_state = LOW;

Bounce debouncer = Bounce();

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer server(80);

void setup() {
/*
 * Need to initialise these from EEPROM
mqtt_broker
mqtt_topic
mqtt_broker_port
 * */
  pinMode(relay_pin, OUTPUT);
  Serial.begin(115200);
  relay_state = LOW;
  debouncer.attach(button_pin, INPUT_PULLUP);
  debouncer.interval(5);
  setup_wifi();
  client.setServer(mqtt_broker, mqtt_broker_port);
  client.setCallback(callback);
  server.on("/",       handle_root);
  server.on("/submit", handle_submit);
  server.begin();
  ArduinoOTA.begin();
}

void handle_root() {
  send_page();
}

void send_page() {
  char local_ip[16];
  sprintf(local_ip, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
  String page = "<!DOCTYPE html><html><body><form action='http://";
  page += local_ip;
  page += "/submit' method='POST'>MQTT broker: <input type='text' name='broker' value='";
  page += mqtt_broker;
  page += "'><br>MQTT broker port: <input type='text' name='broker_port' value='";
  page += mqtt_broker_port;
  page += "'><br>MQTT topic: <input type='text' name='topic' value='";
  page += mqtt_topic;
  page += "'><br><input type='submit' value='Submit'></form></body></html>";
  server.send(200, "text/html", page);
}
void handle_submit() {
  if (server.args() > 0 ) {
    for ( uint8_t i = 0; i < server.args(); i++ ) {
      if (server.argName(i) == "broker") {
        client.disconnect();
        server.arg(i).toCharArray(mqtt_broker, 50);
        client.setServer(mqtt_broker, mqtt_broker_port);
      } else if (server.argName(i) == "broker_port") {
        client.disconnect();
        mqtt_broker_port = server.arg(i).toInt();
        client.setServer(mqtt_broker, mqtt_broker_port);
      } else if (server.argName(i) == "topic") {
        client.unsubscribe(mqtt_topic);
        server.arg(i).toCharArray(mqtt_topic, 50);
        client.subscribe(mqtt_topic);
      }
    }
  }
  send_page();
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if ((char)payload[0] == '0') {
    digitalWrite(relay_pin, LOW);   // Turn the LED on (Note that LOW is the voltage level
    Serial.println("relay_pin -> LOW");
    relay_state = LOW;
  } else if ((char)payload[0] == '1') {
    digitalWrite(relay_pin, HIGH);  // Turn the LED off by making the voltage HIGH
    Serial.println("relay_pin -> HIGH");
    relay_state = HIGH;
  }
}

void mqtt_connect() {
  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (client.connect("ESP8266Client")) {
    Serial.println("connected");
    // Once connected, publish an announcement...
    client.publish(mqtt_topic, "LED 1 is connected");
    // ... and resubscribe
    client.subscribe(mqtt_topic);
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
}

void loop() {
  unsigned long current_millis = millis();
  if (!client.connected() && current_millis - previous_millis >= 5000) {
    previous_millis = current_millis;
    mqtt_connect();
  }
  ArduinoOTA.handle();
  server.handleClient();

  if(client.connected()) {
    client.loop();
  }

  debouncer.update();
  if ( debouncer.fell()) {
    Serial.println("Debouncer fell");
    // Toggle relay state :
    relay_state = !relay_state;
    digitalWrite(relay_pin, relay_state);
    if (relay_state == HIGH) {
      client.publish(mqtt_topic, "1");
    }
    else if (relay_state == LOW) {
      client.publish(mqtt_topic, "0");
    }
  }
}


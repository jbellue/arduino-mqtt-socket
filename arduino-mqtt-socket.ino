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


const char* mqtt_server = "192.168.1.24";
const char* mqtt_topic = "led_1";

const int relay_pin = D2;
const int button_pin = D5;
bool relay_state = LOW;

Bounce debouncer = Bounce();

WiFiClient espClient;
ESP8266WebServer server(80);

PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

void setup() {
  pinMode(relay_pin, OUTPUT);
  Serial.begin(115200);
  relay_state = LOW;
  debouncer.attach(button_pin, INPUT_PULLUP);
  debouncer.interval(5);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
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
  page += mqtt_server;
  page += "'><br>MQTT topic: <input type='text' name='topic' value='";
  page += mqtt_topic;
  page += "'><br>Relay:<br><input type='radio' name='relay' value='on'";
  if(relay_state == HIGH) {
    page += " checked";
  }
  page += ">on<br><input type='radio' name='relay' value='off'";
    if(relay_state == LOW) {
    page += " checked";
  }
  page += ">off<br><br><input type='submit' value='Submit'></form></body></html>";
  server.send(200, "text/html", page);
}
void handle_submit() {
  if (server.args() > 0 ) {
    for ( uint8_t i = 0; i < server.args(); i++ ) {
      if (server.argName(i) == "broker") {
        
      } else if (server.argName(i) == "topic") {
        
      } else if (server.argName(i) == "relay") {
        if (server.arg(i) == "on") {
          relay_state = HIGH;
          client.publish(mqtt_topic, "1");
        }
        else {
          relay_state = LOW;
          client.publish(mqtt_topic, "0");
        }
        digitalWrite(relay_pin, relay_state);
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

  // Switch on the LED if an 1 was received as first character
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

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
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
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  ArduinoOTA.handle();
  client.loop();
  server.handleClient();

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


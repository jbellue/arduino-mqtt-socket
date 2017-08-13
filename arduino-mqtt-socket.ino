#include <WiFiManager.h>

#include <Bounce2.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoJson.h>
#include "FS.h"

#define BUFFER_LENGTH 100
char mqtt_broker[BUFFER_LENGTH] = "";
char mqtt_topic[BUFFER_LENGTH] = "";
int mqtt_broker_port = 0;
bool mqtt_retain = false;

unsigned long previous_millis = 0;

const int relay_pin = D2;
const int button_pin = D5;
bool relay_state = LOW;

Bounce debouncer = Bounce();

WiFiClient espClient;
PubSubClient client(espClient);
ESP8266WebServer server(80);

bool load_config() {
    File config_file = SPIFFS.open("/config.json", "r");
    if (!config_file) {
        Serial.println("Failed to open config file");
        return false;
    }

    size_t size = config_file.size();
    if (size > 1024) {
        Serial.println("Config file size is too large");
        config_file.close();
        return false;
    }

    std::unique_ptr<char[]> buf(new char[size]);
    config_file.readBytes(buf.get(), size);

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());

    if (!json.success()) {
        Serial.println("Failed to parse config file");
        config_file.close();
        return false;
    }

    strncpy(mqtt_broker, json["mqtt_broker"], sizeof(mqtt_broker));
    mqtt_broker_port = json["mqtt_broker_port"];
    strncpy(mqtt_topic, json["mqtt_topic"], sizeof(mqtt_broker));
    mqtt_retain = json["mqtt_retain"];

    config_file.close();
    return true;
}

bool save_config() {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_broker"] = mqtt_broker;
    json["mqtt_topic"] = mqtt_topic;
    json["mqtt_broker_port"] = mqtt_broker_port;
    json["mqtt_retain"] = mqtt_retain;

    File config_file = SPIFFS.open("/config.json", "w");
    if (!config_file) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    json.printTo(config_file);
    config_file.close();
    return true;
}

void setup() {
    WiFiManager wifiManager;
    wifiManager.autoConnect("AutoConnectAP");
    pinMode(relay_pin, OUTPUT);
    Serial.begin(115200);
    relay_state = LOW;
    debouncer.attach(button_pin, INPUT_PULLUP);
    debouncer.interval(5);

    SPIFFS.begin();

    if (!load_config()) {
        Serial.println("Failed to load config");
    } else {
        Serial.println("Config loaded");
    }

    client.setServer(mqtt_broker, mqtt_broker_port);
    client.setCallback(callback);
    server.on("/", handle_request);
    server.begin();
    ArduinoOTA.begin();
}

void send_page() {
    char local_ip[16];
    sprintf(local_ip, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
    String page = "<!DOCTYPE html><html><body><form action='http://";
    page += local_ip;
    page += "/' method='POST'>MQTT broker: <input type='text' name='broker' value='";
    page += mqtt_broker;
    page += "'><br>MQTT broker port: <input type='text' name='broker_port' value='";
    page += mqtt_broker_port;
    page += "'><br>MQTT topic: <input type='text' name='topic' value='";
    page += mqtt_topic;
    page += "'><br>MQTT message retain: <input type='checkbox' name='retain'";
    if (mqtt_retain) page += "checked='checked'";
    page += "'><br><input type='submit' value='Submit'></form></body></html>";
    server.send(200, "text/html", page);
}

void handle_request() {
    if (server.args() > 0 ) {
        for ( uint8_t i = 0; i < server.args(); i++ ) {
            if (server.argName(i) == "broker") {
                client.disconnect();
                server.arg(i).toCharArray(mqtt_broker, BUFFER_LENGTH);
                client.setServer(mqtt_broker, mqtt_broker_port);
            } else if (server.argName(i) == "broker_port") {
                client.disconnect();
                mqtt_broker_port = server.arg(i).toInt();
                client.setServer(mqtt_broker, mqtt_broker_port);
            } else if (server.argName(i) == "topic") {
                client.unsubscribe(mqtt_topic);
                server.arg(i).toCharArray(mqtt_topic, BUFFER_LENGTH);
                client.subscribe(mqtt_topic);
            } else if (server.argName(i) == "retain") {
                mqtt_retain = true;
            }
        }
        save_config();
    }
    send_page();
}

void callback(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();
    if(0 == strcmp(topic, mqtt_topic)) {
        if ((char)payload[0] == '0') {
            Serial.println("relay_pin -> LOW");
            relay_state = LOW;
        } else if ((char)payload[0] == '1') {
            Serial.println("relay_pin -> HIGH");
            relay_state = HIGH;
        } else if ((char)payload[0] == '2') {
            relay_state = !relay_state;
            Serial.println("relay_pin -> HIGH");
        }
        digitalWrite(relay_pin, relay_state);
    }
}

void mqtt_connect() {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")) {
        Serial.println("connected");
        client.publish(mqtt_topic, "LED 1 is connected");
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
            client.publish(mqtt_topic, "1", mqtt_retain);
        }
        else if (relay_state == LOW) {
            client.publish(mqtt_topic, "0", mqtt_retain);
        }
    }
}


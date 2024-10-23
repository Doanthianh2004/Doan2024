#include <DHT22.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>


#define camBienMua 4
#define camBienAnhSang 2
#define RELAY_PIN 18
// WiFi
const char *ssid = "VNA";
const char *password = "11223344@";

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const char *topic = "esp32/led";
//const char *mqtt_relay_topic = "esp32/relay";
const char *mqtt_username = "emqx";
const char *mqtt_password = "public";
const int mqtt_port = 1883;

// DHT sensor setup
#define DHTPIN 14
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
int relay = 0;
// BMP180 sensor setup
Adafruit_BMP085 bmp;
float seaLevelPressure = 101325.0;

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastPublishTime = 0;
const long publishInterval = 2000;

void reconnectMQTT() {
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Public EMQX MQTT broker connected");
      client.subscribe(topic);
    } else {
      Serial.print("Failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.println("Reconnecting to WiFi...");
    }
    Serial.println("Reconnected to the WiFi network");
  }
}

void setup() {
  Serial.begin(9600);
  delay(1000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to the WiFi network");

  pinMode(camBienAnhSang, INPUT);
  pinMode(camBienMua, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  dht.begin();

  if (!bmp.begin()) {
    Serial.println("Không tìm thấy cảm biến BMP180, vui lòng kiểm tra kết nối!");
    while (1) {}
  }
  Serial.println("Cảm biến BMP180 đã sẵn sàng.");

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);

  reconnectMQTT();
}

void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  //Serial.print("?????");
  if (message == "1" && !relay) {
    //digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Relay ON");
    relay = 1;
  } else if (message == "0" && relay) {
    //digitalWrite(RELAY_PIN, LOW);
    Serial.println("Relay OFF");
    relay = 0;
  }
}

void loop() {
  reconnectWiFi();
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  unsigned long currentTime = millis();
  if(relay == 0){
    digitalWrite(RELAY_PIN, LOW);
  }else{
    digitalWrite(RELAY_PIN, HIGH);
  }

  if (currentTime - lastPublishTime >= publishInterval) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    float f = dht.readTemperature(true);

    if (isnan(h) || isnan(t) || isnan(f)) {
      Serial.println("Failed to read from DHT sensor!");
    }

    int mua = digitalRead(camBienMua);
    int anhsang = digitalRead(camBienAnhSang);

    float temperatureBMP = bmp.readTemperature();
    float pressure = bmp.readPressure();
    float adjustedPressure = pressure * (seaLevelPressure / 101325.0);

    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print(" *C ");
    Serial.print(f);
    Serial.print(" *F\t");
    Serial.print("  Heat index: ");
    float hic = dht.computeHeatIndex(t, h, false);
    Serial.print(hic);
    Serial.print(" *C ");
    Serial.println();

    Serial.print("Mưa: ");
    Serial.println(mua);
    Serial.print("Ánh sáng: ");
    Serial.println(anhsang);
    Serial.print("Nhiệt độ BMP180: ");
    Serial.print(temperatureBMP);
    Serial.println(" °C");
    Serial.print("Áp suất: ");
    Serial.print(adjustedPressure);
    Serial.println(" Pa");

    DynamicJsonDocument data(1024);
    data["humidity"] = h;
    data["temperature"] = t;
    data["rain"] = mua;
    data["light"] = anhsang;
    data["bmp_temperature"] = temperatureBMP;
    data["pressure"] = adjustedPressure;
    data["pump"] = relay;
    char json_string[1024];
    serializeJson(data, json_string);
    Serial.println(json_string);

    client.publish(topic, json_string, false);

    lastPublishTime = currentTime;
  }
}
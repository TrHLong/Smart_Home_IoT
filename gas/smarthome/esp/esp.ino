#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// --- WiFi + MQTT ---
const char* ssid = "WIFI SINH VIEN";
const char* password = "";
const char* mqtt_server = "78d21cf93a6b464fb94f091a2e745cee.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_user = "ESP8266B";
const char* mqtt_pass = "DoorHome1";

WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

unsigned long lastMqttPing = 0;
const unsigned long MQTT_PING_INTERVAL = 30000;

// --- Serial read buffer ---
#define BUF_SIZE 64
char inBuffer[BUF_SIZE];
uint8_t bufIndex = 0;

// --- Kết nối MQTT ---
void publishIfConnected(const char* topic, const char* payload) {
  if(client.connected()) client.publish(topic, payload);
}

// --- Xử lý lệnh từ MQTT --- 
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for(unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  Serial.print("[MQTT->UNO] Topic: "); Serial.println(topic);
  Serial.print("[MQTT->UNO] Payload: "); Serial.println(msg);

  // Gửi trực tiếp xuống UNO
  Serial.println(msg);
}

// --- Kết nối lại MQTT ---
void reconnect() {
  static unsigned long lastAttempt = 0;
  if(millis() - lastAttempt < 5000) return;
  lastAttempt = millis();

  if(WiFi.status() != WL_CONNECTED){
    WiFi.reconnect();
    return;
  }

  Serial.print("Kết nối MQTT...");
  if(client.connect("esp8266_client", mqtt_user, mqtt_pass)) {
    Serial.println("OK!");
    client.subscribe("nha/gas/lenh"); // nhận lệnh từ cloud
    publishIfConnected("nha/gas/log", "esp_online");
  } else {
    Serial.print("Lỗi: "); Serial.println(client.state());
  }
}

// --- Đọc dữ liệu từ UNO ---
String readUNO() {
  while(Serial.available()) {
    char c = Serial.read();
    if(c == '\n' || c == '\r') {
      if(bufIndex > 0){
        inBuffer[bufIndex] = 0;
        String line = String(inBuffer);
        line.trim();
        bufIndex = 0;
        return line;
      }
    } else if(bufIndex < BUF_SIZE - 1){
      inBuffer[bufIndex++] = c;
    }
  }
  return "";
}

// --- Setup ---
void setup() {
  Serial.begin(9600); // Serial0 để kết nối với UNO
  delay(200);

  // --- WiFi ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Đang kết nối WiFi");
  while(WiFi.status() != WL_CONNECTED){
    delay(250);
    Serial.print(".");
  }
  Serial.println("\nWiFi OK! IP: "); Serial.println(WiFi.localIP());

  // --- MQTT ---
  wifiClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("ESP sẵn sàng");
}

// --- Loop ---
void loop() {
  // --- reconnect nếu MQTT rớt ---
  if(!client.connected()) reconnect();
  client.loop();

  // --- Đọc Serial từ UNO ---
  String line = readUNO();
  if(line.length() > 0){
    Serial.print("[UNO->MQTT] "); Serial.println(line);
    String lower = line;
    lower.toLowerCase();

    // Publish lên topic tương ứng
    if(lower.startsWith("gas_value:")) publishIfConnected("nha/gas/level", line.c_str());
    else if(lower.startsWith("gas_servo:")) publishIfConnected("nha/gas/servo", line.c_str());
    else if(lower.startsWith("led_q:")) publishIfConnected("nha/gas/ledquang", line.c_str());
    else if(lower.startsWith("alert:")) publishIfConnected("nha/gas/alert", line.c_str());
    else publishIfConnected("nha/gas/log", line.c_str());
  }

  // --- Ping broker định kỳ ---
  if(millis() - lastMqttPing >= MQTT_PING_INTERVAL){
    lastMqttPing = millis();
    if(client.connected()) publishIfConnected("nha/gas/log", "ping");
  }
}

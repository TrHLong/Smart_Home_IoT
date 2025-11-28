#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <SoftwareSerial.h>

// SoftwareSerial: RX, TX (UNO → ESP)
SoftwareSerial UNO(14, 12);   // D5 = RX (14), D6 = TX (12)

// ====== WiFi + MQTT ======
const char* ssid = "WIFI SINH VIEN";
const char* password = "";

const char* mqtt_server = "olivecarder-4742ce08.a01.euc1.aws.hivemq.cloud";
const int   mqtt_port   = 8883;
const char* mqtt_user   = "ESP8266A";
const char* mqtt_pass   = "DoorHome1";

WiFiClientSecure wifiClient;
PubSubClient client(wifiClient);

unsigned long lastMqttPing = 0;
const unsigned long MQTT_PING_INTERVAL = 30000UL;

// ---------------------------------------------------
//      Publish an toàn (chỉ khi đã connected)
// ---------------------------------------------------
void publishIfConnected(const char* topic, const char* payload) {
  if (client.connected()) client.publish(topic, payload);
}

// ---------------------------------------------------
//                Nhận lệnh từ MQTT
// ---------------------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();

  Serial.print("[MQTT] Nhận: ");
  Serial.println(msg);

  // Gửi xuống UNO (điều khiển thực tế)
  UNO.println(msg);
}

// ---------------------------------------------------
void reconnect() {
  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < 5000) return;
  lastAttempt = millis();

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return;
  }

  Serial.print("Kết nối MQTT...");
  if (client.connect("ESP8266_Client", mqtt_user, mqtt_pass)) {
    Serial.println("OK!");

    // ==== ĐĂNG KÝ TOPIC LỆNH ====
    client.subscribe("nha/cua/lenh");        // mo_cua / dong_cua
    client.subscribe("nha/phoi_do/lenh");    // phoi / thu

    // ==== LỆNH MỚI ====
    client.subscribe("nha/cuasau/lenh");     // mo_cuasau / dong_cuasau
    client.subscribe("nha/rgb/lenh");        // rgb_on / rgb_off

    publishIfConnected("nha/hethong/log", "esp_online");

  } else {
    Serial.print("Lỗi: ");
    Serial.println(client.state());
  }
}

// ---------------------------------------------------
String readUNO() {
  if (!UNO.available()) return "";
  String s = UNO.readStringUntil('\n');
  s.trim();
  return s;
}

// ---------------------------------------------------
void setup() {
  Serial.begin(9600);
  UNO.begin(9600);
  delay(200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Đang kết nối WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  Serial.println("\nWiFi OK!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  wifiClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("ESP sẵn sàng.");
}// ---------------------------------------------------
void loop() {

  if (!client.connected()) reconnect();
  client.loop();

  // ==================================================================
  //        NHẬN DỮ LIỆU TỪ UNO → PUBLISH MQTT
  // ==================================================================
  String line = readUNO();
  if (line.length()) {

    Serial.print("[UNO->ESP] ");
    Serial.println(line);

    // ===== THỜI TIẾT =====
    if (line == "mua" || line == "tanh")
      publishIfConnected("nha/mua/trangthai", line.c_str());

    // ===== CỬA CHÍNH =====
    else if (line == "mo_cua" || line == "dong_cua")
      publishIfConnected("nha/cua/trangthai", line.c_str());

    // ===== PHƠI ĐỒ =====
    else if (line == "phoi" || line == "thu")
      publishIfConnected("nha/quanao/trangthai", line.c_str());

    // ===== CỬA SAU (NEW) =====
    else if (line == "mo_cuasau" || line == "dong_cuasau")
      publishIfConnected("nha/cuasau/trangthai", line.c_str());

    // ===== LED RGB (NEW) =====
    else if (line == "rgb_on" || line == "rgb_off")
      publishIfConnected("nha/rgb/trangthai", line.c_str());
  }

  // ===== MQTT ping =====
  if (millis() - lastMqttPing >= MQTT_PING_INTERVAL) {
    lastMqttPing = millis();
    if (client.connected())
      client.publish("nha/hethong/log", "ping");
  }
}
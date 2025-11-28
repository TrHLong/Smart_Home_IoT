#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ==== KHAI BÁO CHÂN ====
#define SS_PIN 10
#define RST_PIN 9
#define SERVO_DOOR_PIN 6     // Servo cửa RFID
#define SERVO_CLOTH_PIN 7    // Servo phơi đồ
#define RAIN_PIN 2           // Cảm biến mưa (Digital S)

// ==== CỬA SAU ====
#define BACKDOOR_SERVO_PIN 3
#define BACKDOOR_BUTTON_PIN 4  

// ==== LED RGB ====
#define LED_R_PIN 5
#define LED_G_PIN A0
#define LED_B_PIN A1
#define LED_BUTTON_PIN A2

// ==== ĐỐI TƯỢNG ====
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo doorServo;
Servo clothServo;
Servo backDoorServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==== BIẾN ====
bool doorOpen = false;
unsigned long openTime = 0;
byte lastUID[4] = {0};

bool isRaining = false;
int rainState = HIGH;

int servoDoorOpen  = 90;
int servoDoorClose = 0;
int servoClothOpen = 90;
int servoClothClose = 0;

// ====== BIẾN CỬA SAU ======
bool backDoorOpen = false;
bool backButtonPressed = false;
bool lastBackButton = HIGH;
unsigned long lastDebounceBack = 0;

// ====== BIẾN LED RGB ======
bool ledState = false;
bool ledButtonPressed = false;
bool lastLedButton = HIGH;
unsigned long lastDebounceLed = 0;

void resetRFID();

// ===================================================
//                      SETUP
// ===================================================
void setup() {
  Serial.begin(9600);
  delay(100);

  SPI.begin();
  mfrc522.PCD_Init();

  doorServo.attach(SERVO_DOOR_PIN);
  clothServo.attach(SERVO_CLOTH_PIN);
  backDoorServo.attach(BACKDOOR_SERVO_PIN);

  doorServo.write(servoDoorClose);
  clothServo.write(servoClothOpen);
  backDoorServo.write(0);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("He thong khoi dong");
  delay(2000);

  pinMode(RAIN_PIN, INPUT);

  pinMode(BACKDOOR_BUTTON_PIN, INPUT_PULLUP);

  pinMode(LED_R_PIN, OUTPUT);
  pinMode(LED_G_PIN, OUTPUT);
  pinMode(LED_B_PIN, OUTPUT);
  pinMode(LED_BUTTON_PIN, INPUT_PULLUP);

  Serial.println("hethong_khoi");
  lcd.clear();
  lcd.print("Cho the cua ban");
}

// ===================================================
//                      LOOP
// ===================================================
void loop() {

  // =======================
  // Nhận lệnh từ MQTT (ESP)
  // =======================
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    // ===== CỬA CHÍNH =====
    if (cmd == "mo_cua") {
      doorServo.write(servoDoorOpen);
      doorOpen = true;
      openTime = millis();
      lcd.clear(); lcd.print("Mo cua MQTT");

    } else if (cmd == "dong_cua") {
      doorServo.write(servoDoorClose);
      doorOpen = false;
      lcd.clear(); lcd.print("Dong cua MQTT");
    }

    // ===== QUẦN ÁO =====
    else if (cmd == "phoi") {
      clothServo.write(servoClothOpen);
      lcd.clear(); lcd.print("Phoi do MQTT");

    } else if (cmd == "thu") {
      clothServo.write(servoClothClose);
lcd.clear(); lcd.print("Thu do MQTT");
    }

    // ===== CỬA SAU =====
    else if (cmd == "mo_cuasau") {
      backDoorServo.write(90);
      backDoorOpen = true;
      lcd.clear(); lcd.print("Mo cua sau");

    } else if (cmd == "dong_cuasau") {
      backDoorServo.write(0);
      backDoorOpen = false;
      lcd.clear(); lcd.print("Dong cua sau");
    }

    // ===== LED RGB =====
    else if (cmd == "rgb_on") {
      ledState = true;
      digitalWrite(LED_R_PIN, HIGH);
      digitalWrite(LED_G_PIN, HIGH);
      digitalWrite(LED_B_PIN, HIGH);
      lcd.clear(); lcd.print("LED: ON");

    } else if (cmd == "rgb_off") {
      ledState = false;
      digitalWrite(LED_R_PIN, LOW);
      digitalWrite(LED_G_PIN, LOW);
      digitalWrite(LED_B_PIN, LOW);
      lcd.clear(); lcd.print("LED: OFF");
    }

    delay(300);
  }

  // ======================
  checkRainSystem();
  checkRFIDSystem();
  checkBackDoorButton();
  checkLedButton();

  // ===== Tự đóng cửa RFID sau 2 phút =====
  if (doorOpen && (millis() - openTime >= 120000UL)) {
    doorServo.write(servoDoorClose);
    doorOpen = false;
    Serial.println("dong_cua");
    lcd.clear(); lcd.print("Tu dong dong");
    delay(500);
    lcd.print("Cho the cua ban");
  }
}

// ===================================================
//                HỆ THỐNG CẢM BIẾN MƯA
// ===================================================
void checkRainSystem() {
  int newState = digitalRead(RAIN_PIN);

  if (newState != rainState) {
    rainState = newState;

    if (rainState == LOW) {
      isRaining = true;
      clothServo.write(servoClothClose);
      Serial.println("mua");
      Serial.println("thu");
      lcd.clear(); lcd.print("Phat hien MUA");

    } else {
      isRaining = false;
      Serial.println("tanh");
      lcd.clear(); lcd.print("Troi TANH");
    }

    delay(400);
  }
}

// ===================================================
//                HỆ THỐNG RFID
// ===================================================
void checkRFIDSystem() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  lcd.clear(); lcd.print("The duoc quet");

  byte uid[4];
  for (byte i = 0; i < 4; i++) uid[i] = mfrc522.uid.uidByte[i];

  bool sameCard = true;
  for (byte i = 0; i < 4; i++)
    if (uid[i] != lastUID[i]) sameCard = false;

  if (!doorOpen) {
    doorServo.write(servoDoorOpen);
    doorOpen = true;
    openTime = millis();
    memcpy(lastUID, uid, 4);
    Serial.println("mo_cua");
    lcd.clear(); lcd.print("Cua da mo");

  } else {
    if (sameCard) {
      doorServo.write(servoDoorClose);
      doorOpen = false;
      Serial.println("dong_cua");
      lcd.clear(); lcd.print("Cua da dong");

    } else {
      lcd.clear(); lcd.print("Sai the!");
    }
  }

  delay(500);
  lcd.clear(); lcd.print("Cho the cua ban");

  resetRFID();
}

// ===================================================
void resetRFID() {
  mfrc522.PICC_HaltA();
mfrc522.PCD_StopCrypto1();
  delay(200);
  mfrc522.PCD_Init();
}

// ===================================================
//       HỆ THỐNG CỬA SAU BẰNG NÚT NHẤN
// ===================================================
void checkBackDoorButton() {
  bool reading = digitalRead(BACKDOOR_BUTTON_PIN);

  if (reading != lastBackButton)
    lastDebounceBack = millis();

  if (millis() - lastDebounceBack > 80) {
    if (reading == LOW && !backButtonPressed) {
      backDoorOpen = !backDoorOpen;

      if (backDoorOpen) {
        backDoorServo.write(90);
        Serial.println("mo_cuasau");
      } else {
        backDoorServo.write(0);
        Serial.println("dong_cuasau");
      }

      backButtonPressed = true;
    }
    else if (reading == HIGH) {
      backButtonPressed = false;
    }
  }

  lastBackButton = reading;
}

// ===================================================
//       HỆ THỐNG LED RGB BẰNG NÚT NHẤN
// ===================================================
void checkLedButton() {
  bool reading = digitalRead(LED_BUTTON_PIN);

  if (reading != lastLedButton)
    lastDebounceLed = millis();

  if (millis() - lastDebounceLed > 80) {
    if (reading == LOW && !ledButtonPressed) {
      ledState = !ledState;

      if (ledState) {
        digitalWrite(LED_R_PIN, HIGH);
        digitalWrite(LED_G_PIN, HIGH);
        digitalWrite(LED_B_PIN, HIGH);
        Serial.println("rgb_on");
      } else {
        digitalWrite(LED_R_PIN, LOW);
        digitalWrite(LED_G_PIN, LOW);
        digitalWrite(LED_B_PIN, LOW);
        Serial.println("rgb_off");
      }

      ledButtonPressed = true;
    }
    else if (reading == HIGH) {
      ledButtonPressed = false;
    }
  }

  lastLedButton = reading;
}
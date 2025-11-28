#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// --- LCD I2C ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Pin ---
const int gasSensorPin = A0;
const int buzzerPin = 12;
const int ledGasPin = 7;
const int ledQuangPin = 6;
const int servoGasPin = 9;
const int buttonQuangPin = 2;
const int buttonGasPin = 13;

// --- Ngưỡng ---
const int gasThreshold = 300;

// --- Servo ---
Servo gasServo;

// --- Trạng thái ---
bool doorOpen = false;
bool gasAlert = false;
unsigned long gasDetectedTime = 0;
const unsigned long gasOpenDuration = 5000;
bool gasServoManual = false;
bool ledQuangManual = false;
bool lastButtonQuangState = HIGH;
bool lastButtonGasState = HIGH;

// --- Serial buffer ---
#define BUF_SIZE 32
char inBuffer[BUF_SIZE];
uint8_t bufIndex = 0;

// --- Lưu để gửi ESP ---
int lastGasValue = -1;
bool lastDoorOpen = false;
bool lastGasServoManual = false;
bool lastLedQuangManual = false;

// --- LCD update ---
int lastLCDGas = -1;
bool lastLCDAlert = false;

// ================== LCD ===================
void showGasValue(int gasValue) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Gas: ");
  lcd.print(gasValue);
  lcd.setCursor(0,1);
  lcd.print("Thresh: ");
  lcd.print(gasThreshold);
}

void showGasAlertLCD(int gasValue) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("GAS DETECTED!");
  lcd.setCursor(0,1);
  lcd.print("Value: ");
  lcd.print(gasValue);
}

// ================= ESP Command ==================
void processCommand(const char *cmd) {
  String command = String(cmd);
  command.trim();
  if (command == "LED_Q_ON") {
    ledQuangManual = true;
    digitalWrite(ledQuangPin, HIGH);
    Serial.println("LED_Q:ON");
  } else if (command == "LED_Q_OFF") {
    ledQuangManual = false;
    digitalWrite(ledQuangPin, LOW);
    Serial.println("LED_Q:OFF");
  } else if (command == "GAS_ON") {
    gasServoManual = true;
    gasServo.write(90);
    Serial.println("GAS_SERVO:OPEN");
  } else if (command == "GAS_OFF") {
    gasServoManual = false;
    gasServo.write(0);
    Serial.println("GAS_SERVO:CLOSE");
  }
}

// ================== SETUP =====================
void setup() {
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledGasPin, OUTPUT);
  pinMode(ledQuangPin, OUTPUT);
  pinMode(buttonQuangPin, INPUT_PULLUP);
  pinMode(buttonGasPin, INPUT_PULLUP);

  digitalWrite(ledGasPin, LOW);
  digitalWrite(ledQuangPin, LOW);
  digitalWrite(buzzerPin, LOW);

  gasServo.attach(servoGasPin);
  gasServo.write(0);

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  Serial.println("UNO khoi dong OK.");
}

// ================== LOOP ======================
void loop() {
  // ----- 1. BUTTON LED QUANG -----
  int readingQuang = digitalRead(buttonQuangPin);
  if (readingQuang == LOW && lastButtonQuangState == HIGH) {
    ledQuangManual = !ledQuangManual;
    digitalWrite(ledQuangPin, ledQuangManual);
    Serial.print("LED_Q:");
    Serial.println(ledQuangManual ? "ON":"OFF");
    delay(50);
  }
  lastButtonQuangState = readingQuang;

  // ----- 2. BUTTON SERVO GAS -----
  int readingGas = digitalRead(buttonGasPin);
  if (readingGas == LOW && lastButtonGasState == HIGH) {
    gasServoManual = !gasServoManual;
    gasServo.write(gasServoManual ? 90 : 0);
    Serial.print("GAS_SERVO:");
    Serial.println(gasServoManual ? "OPEN":"CLOSE");
    delay(50);
  }
  lastButtonGasState = readingGas;

  // ----- 3. COMMAND FROM ESP -----
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c=='\n' || c=='\r') {
      if (bufIndex > 0) {
        inBuffer[bufIndex] = 0;
        processCommand(inBuffer);
        bufIndex = 0;
      }
    } else {
      if (bufIndex < BUF_SIZE-1) inBuffer[bufIndex++] = c;
    }
  }

  // ----- 4. READ GAS + PROCESS -----
  int gasValue = analogRead(gasSensorPin);

  if (gasValue > gasThreshold) {
    gasAlert = true;
    // Bật LED Gas
    digitalWrite(ledGasPin, HIGH);
    // Buzzer kêu dài liên tục
    digitalWrite(buzzerPin, HIGH);
    // Mở servo chỉ 1 lần nếu chưa mở
    if (!doorOpen) {
      doorOpen = true;
      gasDetectedTime = millis();
      if (!gasServoManual) gasServo.write(90);
      Serial.println("ON");
    }
  } else {
    gasAlert = false;
    // Tắt LED và buzzer
    digitalWrite(ledGasPin, LOW);
    digitalWrite(buzzerPin, LOW);
    // Đóng cửa nếu hết thời gian mở
    if (doorOpen && (millis() - gasDetectedTime > gasOpenDuration)) {
      doorOpen = false;
      if (!gasServoManual) gasServo.write(0);
      Serial.println("OFF");
    }
  }

  // ----- 5. Manual servo override -----
  if (gasServoManual) gasServo.write(90);

  // ----- 6. LCD -----
  if (gasAlert != lastLCDAlert || abs(gasValue - lastLCDGas) > 10) {
    lastLCDGas = gasValue;
    lastLCDAlert = gasAlert;
    if (gasAlert) showGasAlertLCD(gasValue);
    else showGasValue(gasValue);
  }

  // ----- 7. SEND STATE TO ESP -----
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    if (abs(gasValue - lastGasValue) > 10 || lastGasValue == -1) {
      lastGasValue = gasValue;
      Serial.print("GAS_VALUE:");
      Serial.println(gasValue);
    }
    bool servoOpen = gasServoManual || doorOpen;
    if (servoOpen != lastDoorOpen || gasServoManual != lastGasServoManual) {
      lastDoorOpen = servoOpen;
      lastGasServoManual = gasServoManual;
      Serial.print("GAS_SERVO:");
      Serial.println(servoOpen ? "OPEN":"CLOSE");
    }
    if (lastLedQuangManual != ledQuangManual) {
      lastLedQuangManual = ledQuangManual;
      Serial.print("LED_Q:");
      Serial.println(ledQuangManual ? "ON":"OFF");
    }
  }

  delay(5);
}
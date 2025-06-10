#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>

#define RX_PIN GPIO_NUM_18
#define TX_PIN GPIO_NUM_19
HardwareSerial mySerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

uint8_t id;  // Biến ID để quản lý các vân tay
const int button = 15;
boolean buttonState = HIGH;
boolean mode = 0; // 0: chế độ quét vân tay, 1: chế độ lưu vân tay
const int ledControl = 23;
unsigned long timeWait = millis();
unsigned long timeReset = millis();
const int buzzer = 4;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// Wi-Fi credentials
const char* ssid = "Hí lô";
const char* password = "12345678d";
const char* serverUrl = "http://192.168.43.202:80/biometricattendace/getdata.php";
String getData, Link;
const char* device_token = "e29db4662a567bba";

int FingerID = 0, t1, t2;                                  // The Fingerprint ID from the scanner 
bool device_Mode = false;                           // Default Mode Enrollment
bool firstConnect = false;
unsigned long previousMillis = 0;

void setup() {
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(100);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Welcome to");
  lcd.setCursor(0, 1);
  lcd.print("Smart lock!");

  pinMode(button, INPUT_PULLUP);
  pinMode(ledControl, OUTPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(ledControl, LOW);
  digitalWrite(buzzer, LOW);
  Serial.println("\n\nAdafruit Fingerprint sensor enrollment");
  finger.begin(57600);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }
  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x")); Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x")); Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: ")); Serial.println(finger.capacity);
  Serial.print(F("Security level: ")); Serial.println(finger.security_level);
  Serial.print(F("Device address: ")); Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: ")); Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: ")); Serial.println(finger.baud_rate);

  // Khởi tạo ID dựa trên số lượng vân tay đã lưu
  id = finger.getTemplateCount();
  Serial.print(F("Templates in memory: ")); Serial.println(id);

  lcd.clear();
  
  // Connect to Wi-Fi
  connectToWiFi();
}

void loop() {
  if ((digitalRead(ledControl) == HIGH) && (millis() - timeWait > 5 * 1000)) {
    digitalWrite(ledControl, LOW);
    lcd.clear();
  }

  // Chuyển đổi chế độ bằng cách nhấn và giữ nút trong 5 giây
  if (digitalRead(button) == LOW) {
    if (buttonState == HIGH) {
      timeReset = millis();
      Serial.println("Ấn giữ 5 giây để chuyển chế độ!");
      delay(200);
      buttonState = LOW;
    }
    if (millis() - timeReset > 5 * 1000) {
      beep(100);
      beep(100);
      beep(100);
      mode = !mode;  // Chuyển đổi giữa chế độ 0 và 1
      Serial.println(mode ? "Chuyển sang chế độ lưu vân tay" : "Chuyển sang chế độ quét vân tay");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(mode ? "Enroll Mode" : "Scan Mode");
      delay(2000);
      lcd.clear();
      timeReset = millis();
    }
  } else {
    buttonState = HIGH;
    timeReset = millis();
  }

  if (mode == 1) {
    getFingerprintEnroll();
  } else {
    getFingerprintID();
    if (digitalRead(ledControl) == LOW) {
      lcd.setCursor(0, 0);
      lcd.print("Scan fingerprint");
      lcd.setCursor(0, 1);
      lcd.print("to unlock!");
    }
  }
}

//==========chương trình con========================
void beep(int d) {
  digitalWrite(buzzer, HIGH);
  delay(d);
  digitalWrite(buzzer, LOW);
  delay(d);
}

// Thêm vân tay mới vào bộ nhớ
uint8_t getFingerprintEnroll() {
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  lcd.setCursor(0, 0);
  lcd.print("Scan your new");
  lcd.setCursor(0, 1);
  lcd.print("fingerprint!");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    default:
      Serial.println("Error");
      break;
    }
  }
  // OK success!
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    default:
      Serial.println("Error");
      return p;
  }
  Serial.println("Remove finger");
  beep(100);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place same finger");
  lcd.setCursor(0, 1);
  lcd.print("again!!!");
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    default:
      Serial.println("Error");
      break;
    }
  }
  // OK success!
  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    default:
      Serial.println("Unknown error");
      return p;
  }
  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else {
    Serial.println("Error");
    return p;
  }
  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    beep(100);
    beep(100);
    Serial.println("Stored!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("New fingerprint");
    lcd.setCursor(0, 1);
    lcd.print("stored!!!");
    delay(2000);
    id++;
    mode = 0; // Quay lại chế độ quét vân tay sau khi lưu thành công
    lcd.clear();
  } else {
    Serial.println("Error");
    return p;
  }
  return true;
}

// Quét vân tay mở cửa
uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    default:
      Serial.println("Scan image.....!");
      return p;
  }
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    default:
      Serial.println("Error");
      return p;
  }
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
    lcd.setCursor(0, 0);
    lcd.print("Hello! Number: ");
    lcd.setCursor(0, 1);
    lcd.print(finger.fingerID);
    digitalWrite(ledControl, HIGH);
    beep(500);
    timeWait = millis();
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fingerprint not");
    lcd.setCursor(0, 1);
    lcd.print("found!");
    Serial.println("Did not find a match");
    return p;
  }
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting to");
  lcd.setCursor(0, 1);
  lcd.print("Wi-Fi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Connected!");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected to");
  lcd.setCursor(0, 1);
  lcd.print("Wi-Fi!");
  delay(2000);
}

#define BLYNK_TEMPLATE_ID "TMPL6JRQ_0Mkn"
#define BLYNK_TEMPLATE_NAME "LORAWAN"
#define BLYNK_AUTH_TOKEN "B1OwWuaIvYDAuPEq_oz8UL5BiMrnl_zy"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <SPI.h>
#include <LoRa.h>

// Định nghĩa chân LoRa
#define SS 5   // Chân Chip Select (NSS)
#define RST 14 // Chân Reset
#define DIO0 2 // Chân DIO0
// SPI.begin(18, 19, 23, 5) (SCK, MISO, MOSI, SS)
// Định nghĩa chân công tắc và relay
#define RELAY_PIN 13

// Định nghĩa LCD I2C
LiquidCrystal_I2C lcd(0x27, 16, 2); // Địa chỉ I2C 0x27, màn hình 16x2

// Thông tin Wi-Fi
char ssid[] = "KHKT";      // Thay bằng SSID của bạn
char pass[] = "123456789"; // Thay bằng mật khẩu Wi-Fi của bạn
const char ENGINE_FAILURE_STATUS_STR[] = "engine_failure";

bool relayOn = false;                // Biến trạng thái relay
unsigned long lastEmergencyTime = 0; // Thời gian cứu nạn gần nhất
unsigned long relayTimeout = 30000;  // Thời gian khóa relay (5 phút = 300000 ms)
bool relayLocked = false;            // Trạng thái khóa relay

void initSystem(); // Hàm khởi tạo hệ thống
void updateLCD(String line1, String line2, String line3, String line4);
void processLoRaMessages(); // Hàm xử lý tin nhắn LoRa
void handleRelayLogic();    // Hàm xử lý logic relay
void sendAckMessage(const String &shipCode, float lat = 0, float lon = 0);

void setup()
{
  Wire.begin(22, 21);
  Serial.begin(9600); // Khởi tạo Serial Monitor
  initSystem();       // Khởi tạo hệ thống
  // Kết nối Wi-Fi và Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Đang kết nối Wi-Fi...");
    delay(1000);
  }
  Serial.println("Wi-Fi đã kết nối.");
  Blynk.virtualWrite(V0, ">> System is ready. Type OFF to disable relay.\n");
}
// Hàm khởi tạo hệ thống
void initSystem()
{
  // Cấu hình chân relay và công tắc
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW); // Tắt relay mặc định (mức thấp)

  // Khởi tạo LCD I2C
  lcd.init();
  lcd.clear();
  lcd.backlight();
  lcd.print("KHOI TAO OK");

  // Khởi tạo SPI với các chân tùy chỉnh
  SPI.begin(18, 19, 23, 5);    // (SCK, MISO, MOSI, SS)
  LoRa.setPins(SS, RST, DIO0); // Thiết lập các chân LoRa
  Serial.println("Đang khởi tạo LoRa...");

  // Khởi tạo LoRa ở tần số 433 MHz
  while (!LoRa.begin(433E6))
  {
    Serial.println("Khởi tạo LoRa thất bại!");
    lcd.print("Khởi tạo LoRa thất bại");
  }
  Serial.println("LoRa đã được khởi tạo.");
  lcd.clear();
  lcd.print("LoRa OK");

  // Kết nối Wi-Fi và Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Đang kết nối Wi-Fi...");
    delay(1000);
  }
  Serial.println("Wi-Fi đã kết nối.");
}
// Hàm cập nhật LCD
void updateLCD(String line1, String line2 = "", String line3 = "", String line4 = "")
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  lcd.setCursor(0, 2);
  lcd.print(line3);
  lcd.setCursor(0, 3);
  lcd.print(line4);
}
// Hàm xử lý tin nhắn LoRa
void processLoRaMessages()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0)
  {
    String incomingMessage = "";
    while (LoRa.available())
    {
      incomingMessage += (char)LoRa.read();
    }
    Serial.println("[INFO] Received message: " + incomingMessage);

    // Kiểm tra format tin nhắn mới: HELP-boatName-boatId-lat-long-internetStatus
    if (incomingMessage.startsWith("HELP-"))
    {
      Serial.println("[INFO] SOS message detected!");

      // Tách các phần của tin nhắn dựa vào dấu '-'
      int firstDash = incomingMessage.indexOf("-");
      int secondDash = incomingMessage.indexOf("-", firstDash + 1);
      int thirdDash = incomingMessage.indexOf("-", secondDash + 1);
      int fourthDash = incomingMessage.indexOf("-", thirdDash + 1);
      int fifthDash = incomingMessage.indexOf("-", fourthDash + 1);
      int sixthDash = incomingMessage.indexOf("-", fifthDash + 1);

      if (firstDash == -1 || secondDash == -1 || thirdDash == -1 || fourthDash == -1 || fifthDash == -1 || sixthDash == -1)
      {
        Serial.println("[ERROR] Invalid message format.");
        updateLCD("Tin nhan khong hop le");
        return;
      }

      // Trích xuất thông tin từ tin nhắn
      const String boatName = incomingMessage.substring(firstDash + 1, secondDash);
      const String boatId = incomingMessage.substring(secondDash + 1, thirdDash);
      float shipLat = incomingMessage.substring(thirdDash + 1, fourthDash).toFloat();
      float shipLon = incomingMessage.substring(fourthDash + 1, fifthDash).toFloat();
      bool internetStatus = incomingMessage.substring(fifthDash + 1).toInt();
      const String boatStatus = incomingMessage.substring(sixthDash + 1);

      Serial.println("[INFO] Boat Name: " + boatName);
      Serial.println("[INFO] Boat ID: " + boatId);
      Serial.println("[INFO] Latitude: " + String(shipLat, 6));
      Serial.println("[INFO] Longitude: " + String(shipLon, 6));
      Serial.println("[INFO] Internet Status: " + String(internetStatus));
      Serial.println("[INFO] Boat Status: " + boatStatus);

      // Gửi tín hiệu xác nhận cứu hộ
      sendAckMessage("DAT_LIEN");
      const String googleMapsUrl = "https://www.google.com/maps/search/?api=1&query=" + String(shipLat, 6) + "," + String(shipLon, 6);
      const String message = "Cảnh báo cứu nạn khẩn cấp từ " + boatId + ". Vui lòng truy cập vào: " + googleMapsUrl;
      Blynk.logEvent("cuu_nan_khan_cap", message); // Gửi thông báo đến Blynk
      Blynk.virtualWrite(V0, message);
      updateLCD(boatName, "-" + boatStatus == ENGINE_FAILURE_STATUS_STR ? "hong dong co" : "chim");
      // Nếu relay không bị khóa, bật relay
      if (!relayLocked)
      {
        digitalWrite(RELAY_PIN, HIGH); // Bật relay
        relayOn = true;
        Serial.println("BUZZ ON");
      }
      else
      {
        Serial.println("BUZZ is locked.");
      }
    }
    else
    {
      Serial.println("[INFO] Invalid message (Does not start with HELP-).");
      updateLCD("Tin nhan khong hop le");
    }
  }
}
// Hàm xử lý logic relay
void handleRelayLogic()
{
  // Kiểm tra nếu relay đã bị khóa và đủ thời gian để mở khóa
  if (relayLocked && millis() - lastEmergencyTime >= relayTimeout)
  {
    relayLocked = false; // Mở khóa relay
    Serial.println("Relay unlocked.");
    Blynk.virtualWrite(V0, ">> Relay unlocked.\n");
  }
}

// Hàm xử lý lệnh từ Terminal (V0)
BLYNK_WRITE(V0)
{
  String command = param.asStr();                 // Đọc lệnh từ Terminal
  Serial.println("Command received: " + command); // Ghi nhận lệnh vào Serial Monitor

  if (command.equalsIgnoreCase("OFFBUZZ"))
  {
    if (relayOn)
    {
      digitalWrite(RELAY_PIN, LOW); // Tắt relay
      relayOn = false;
      relayLocked = true;           // Khóa relay
      lastEmergencyTime = millis(); // Ghi nhận thời gian
      Serial.println("BUZZ OFF and locked via Terminal.");
      Blynk.virtualWrite(V0, ">> BUZZ OFF and locked.\n");
    }
    else
    {
      Blynk.virtualWrite(V0, ">> BUZZ is already OFF.\n");
    }
  }
  else
  {
    Blynk.virtualWrite(V0, ">> Invalid command. Use OFFBUZZ.\n");
  }
}

void loop()
{
  Blynk.run();           // Cập nhật Blynk
  processLoRaMessages(); // Xử lý tin nhắn LoRa
  handleRelayLogic();    // Xử lý logic relay
}
// Hàm gửi phản hồi qua LoRa
void sendAckMessage(const String &shipCode, float lat, float lon)
{
  String ackMessage = "HELP_RES-" + shipCode + "-" + String(lat, 6) + "-" + String(lon, 6);
  LoRa.beginPacket();
  LoRa.print(ackMessage);
  LoRa.endPacket();

  Serial.println("[INFO] Acknowledgment sent: " + ackMessage);
}
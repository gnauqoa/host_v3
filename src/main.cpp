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

bool relayOn = false;                // Biến trạng thái relay
unsigned long lastEmergencyTime = 0; // Thời gian cứu nạn gần nhất
unsigned long relayTimeout = 30000;  // Thời gian khóa relay (5 phút = 300000 ms)
bool relayLocked = false;            // Trạng thái khóa relay

void initSystem(); // Hàm khởi tạo hệ thống
void updateLCD(String senderId);
void processLoRaMessages(); // Hàm xử lý tin nhắn LoRa
void handleRelayLogic();    // Hàm xử lý logic relay
void sendAckMessage(const String &shipCode); // Hàm gửi phản hồi qua LoRa

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
void updateLCD(String senderId)
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CUU NAN KHAN CAP!");
  lcd.setCursor(0, 1);
  lcd.print("MA TAU: " + senderId);
}
// Hàm xử lý tin nhắn LoRa
void processLoRaMessages()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    String receivedMessage = "";
    while (LoRa.available())
    {
      receivedMessage += (char)LoRa.read(); // Đọc thông điệp từ LoRa
    }
    Serial.println("Received Message: " + receivedMessage); // In thông điệp lên Serial Monitor

    // Kiểm tra nếu tin nhắn bắt đầu bằng "LORACUUNAN-"
    if (receivedMessage.startsWith("LORACUUNAN"))
    {
      // Gửi phản hồi
      sendAckMessage("DAT_LIEN");

      int separatorIndex1 = receivedMessage.indexOf("-");
      if (separatorIndex1 != -1)
      {
        String senderId = receivedMessage.substring(separatorIndex1 + 1, 14); // Lấy mã định danh
        if (senderId.length() > 0)
        {
          int separatorIndex2 = receivedMessage.indexOf("-", separatorIndex1 + 1);
          if (separatorIndex2 != -1)
          {
            String url = receivedMessage.substring(separatorIndex2 + 1); // Lấy URL từ dấu "-" thứ hai
            String message = "Cảnh báo cứu nạn khẩn cấp từ " + senderId + ". Vui lòng truy cập vào: " + url;
            Blynk.logEvent("cuu_nan_khan_cap", message); // Gửi thông báo đến Blynk
            Blynk.virtualWrite(V0, message);
            updateLCD(senderId); // Cập nhật thông tin lên LCD
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
        }
      }
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
void sendAckMessage(const String &shipCode)
{
  String ackMessage = "DANHANCUUNAN_" + shipCode;
  LoRa.beginPacket();
  LoRa.print(ackMessage);
  LoRa.endPacket();

  Serial.println("[INFO] Đã gửi phản hồi: " + ackMessage);
}
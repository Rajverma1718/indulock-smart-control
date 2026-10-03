#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

// ESP32 Industrial Access Control & Tool Crib Management System
// Demo credentials: RFID card UID is checked against an approved employee.

constexpr uint8_t RFID_SS = 5, RFID_RST = 27, SERVO_PIN = 4;
constexpr uint8_t BUZZER_PIN = 12, GREEN_LED = 2, RED_LED = 15, EMERGENCY_PIN = 34;
MFRC522 rfid(RFID_SS, RFID_RST);
LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 rtc;
Servo doorLock;
WebServer server(80);

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {{'1','2','3','A'},{'4','5','6','B'},{'7','8','9','C'},{'*','0','#','D'}};
byte rowPins[ROWS] = {32, 33, 25, 26};
byte colPins[COLS] = {13, 14, 16, 17};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

struct Tool { const char* id; const char* name; bool checkedOut; const char* holder; };
Tool tools[] = {{"T01", "Torque Wrench", false, ""}, {"T02", "Digital Meter", false, ""}, {"T03", "Safety Helmet", false, ""}};
String audit[12]; uint8_t auditCount = 0;
String pin = ""; bool cardApproved = false; bool cribUnlocked = false; uint8_t selectedTool = 0;
uint32_t lockDeadline = 0, lastLcd = 0;

void addAudit(const String& event) {
  DateTime now = rtc.now();
  char stamp[24]; snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  String line = String(stamp) + " | " + event;
  if (auditCount < 12) audit[auditCount++] = line; else { for (uint8_t i=1;i<12;i++) audit[i-1]=audit[i]; audit[11]=line; }
  Serial.println(line);
}

void show(const String& a, const String& b="") { lcd.clear(); lcd.setCursor(0,0); lcd.print(a.substring(0,16)); lcd.setCursor(0,1); lcd.print(b.substring(0,16)); }

void alarm() { digitalWrite(RED_LED, HIGH); for (int i=0;i<3;i++){ tone(BUZZER_PIN, 2200, 120); delay(170); } digitalWrite(RED_LED, LOW); }

void unlockDoor() { doorLock.write(90); cribUnlocked=true; lockDeadline=millis()+8000; digitalWrite(GREEN_LED,HIGH); show("ACCESS GRANTED","Door unlocked"); addAudit("EMP102 access granted"); }
void lockDoor() { doorLock.write(0); cribUnlocked=false; digitalWrite(GREEN_LED,LOW); }

bool approvedCard() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return false;
  // Wokwi/demo employee card: accept any four-byte card UID for easy simulation.
  String uid=""; for (byte i=0;i<rfid.uid.size;i++){ if(rfid.uid.uidByte[i]<16) uid+="0"; uid+=String(rfid.uid.uidByte[i],HEX); }
  uid.toUpperCase(); rfid.PICC_HaltA(); rfid.PCD_StopCrypto1();
  cardApproved=true; show("CARD OK", "ENTER PIN"); addAudit("RFID accepted UID "+uid); return true;
}

void processKey(char k) {
  if (!cardApproved) return;
  if (k=='#') { if(pin=="1234") unlockDoor(); else { show("ACCESS DENIED","Bad PIN"); addAudit("Invalid PIN"); alarm(); } pin=""; cardApproved=false; return; }
  if (k=='*') { pin=""; show("PIN CLEARED","ENTER PIN"); return; }
  if (k>='0' && k<='9' && pin.length()<4) { pin+=k; lcd.setCursor(pin.length()-1,1); lcd.print('*'); }
}

void toolAction(uint8_t idx) { if(!cribUnlocked) return; tools[idx].checkedOut=!tools[idx].checkedOut; tools[idx].holder=tools[idx].checkedOut?"EMP102":""; addAudit(String(tools[idx].id)+" "+(tools[idx].checkedOut?"checked out":"returned")); show(tools[idx].name, tools[idx].checkedOut?"CHECKED OUT":"RETURNED"); }

String jsonData() { String j = "{\"door\":"; j += cribUnlocked ? "\"OPEN\"" : "\"LOCKED\""; j += ",\"events\":"; j += String(auditCount); j += ",\"tools\":["; for (uint8_t i=0;i<3;i++) { if (i) j += ","; j += "{\"id\":\""; j += String(tools[i].id); j += "\",\"name\":\""; j += String(tools[i].name); j += "\",\"status\":\""; j += tools[i].checkedOut ? "OUT" : "AVAILABLE"; j += "\"}"; } return j + "]}"; }
void dashboard() { String h="<html><meta name='viewport' content='width=device-width'><style>body{font-family:Arial;background:#111827;color:#e5e7eb;margin:24px}.card{background:#1f2937;padding:18px;border-radius:12px;margin:12px 0}h1{color:#60a5fa}.ok{color:#4ade80}.bad{color:#fb7185}</style><h1>Industrial Tool Crib</h1><div class='card'>Door: <b id='d'>--</b><br>Audit events: <span id='e'>--</span></div><div class='card' id='tools'>Loading...</div><script>async function r(){let x=await fetch('/api/status').then(a=>a.json());d.textContent=x.door;e.textContent=x.events;tools.innerHTML=x.tools.map(t=>t.id+' - '+t.name+' : '+t.status).join('<br>')}setInterval(r,1000);r()</script>"; server.send(200,"text/html",h); }

void setup() { Serial.begin(115200); pinMode(GREEN_LED,OUTPUT);pinMode(RED_LED,OUTPUT);pinMode(BUZZER_PIN,OUTPUT);pinMode(EMERGENCY_PIN,INPUT); SPI.begin(); rfid.PCD_Init(); Wire.begin(21,22); lcd.init();lcd.backlight(); rtc.begin(); doorLock.attach(SERVO_PIN);lockDoor(); show("TOOL CRIB READY","SCAN RFID"); WiFi.mode(WIFI_STA);WiFi.begin("Wokwi-GUEST",""); server.on("/",dashboard);server.on("/api/status",[](){server.send(200,"application/json",jsonData());});server.begin(); addAudit("System started"); }

void loop() { if(digitalRead(EMERGENCY_PIN)) { lockDoor(); show("EMERGENCY LOCK","SYSTEM SECURE"); addAudit("Emergency lock input"); delay(500); } approvedCard(); processKey(keypad.getKey()); if(cribUnlocked && millis()>lockDeadline) { lockDoor(); show("DOOR LOCKED","SCAN RFID"); } server.handleClient(); if(millis()-lastLcd>5000 && !cardApproved && !cribUnlocked) {lastLcd=millis();show("SCAN RFID CARD","Tool crib locked");} }

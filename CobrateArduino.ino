#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <PN532.h>
#include <PN532_I2C.h>
#include <NfcAdapter.h>

#define SSID "SLVany"
#define PSK "Prueba123"
#define URL "http://192.168.214.230/"
#define USER "admin"
#define PASS "123"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define I2C_SDA 21
#define I2C_SCL 22
#define LED_ST 2
#define LED_RED 12
#define LED_GREEN 13
#define BUZZER 14
union ArrayToInteger {
  byte array[4];
  uint32_t integer;
} tagId32;

TwoWire WireI2C = TwoWire(0);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &WireI2C, -1);
PN532_I2C pn532_i2c(WireI2C);
PN532 nfc (pn532_i2c);

boolean success;
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
uint8_t uidLength;

WiFiMulti wifiMulti;
String jwt;
bool sesion;

int animacion = 0;

void setup(void) {
  Serial.begin(115200);
  Serial.println("Setup....");
  pinMode(LED_ST, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);

  digitalWrite(LED_ST, LOW);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  
  WireI2C.begin(I2C_SDA, I2C_SCL, 10000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 fallo conexión"));
    for (;;);
  }
  delay(1000);
  
  printLCD("Conectando a la red..");

  wifiMulti.addAP(SSID, PSK);
  sesion = false;
  while(wifiMulti.run() != WL_CONNECTED) ;

  printLCD("Iniciando....");
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.println("Didn't find PN53x board");
    printLCD("ERROR RF");
    delay(1000);
    while (1) ;
  }
  nfc.setPassiveActivationRetries(0xFF);
  nfc.SAMConfig();
  digitalWrite(LED_ST, HIGH);
  printLCD("BIENVENIDO");
  Serial.println("Sistema Inicializado");
}

void loop() {
  readNFC();
}

void readNFC() {
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength, 50);
  Serial.println(success);
  printLCD("BIENVENIDO");
  if(animacion){
    printLCD("=>", 1);
    animacion = 0;
  } 
  else {
    printLCD(" =>", 1);
    animacion = 1;
  }
  if (success) {
    Serial.println(uidLength);
    tagId32.array[0] = uid[0];
    tagId32.array[1] = uid[1];
    tagId32.array[2] = uid[2];
    tagId32.array[3] = uid[3];
    Serial.println(tagId32.integer);
    printLCD("COBRANDO...");
    song();
    if((wifiMulti.run() == WL_CONNECTED)) {
      if (!sesion) login();
      pay();
    }else{
      digitalWrite(LED_RED, HIGH);
      delay(5000);
      printLCD("SIN RED...");
      digitalWrite(LED_RED, LOW);
    }
  }
  delay(500);
}


void tone(byte pin, int freq) {
  ledcSetup(1, 2000, 8);
  ledcAttachPin(pin, 1);
  ledcWriteTone(1, freq);
}
void noTone(byte pin) {
  tone(pin, 0);
}

void printLCD(String x){
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.clearDisplay();
  printLCD(x, 0);
}

void printLCD(String x, int linea){
  display.setCursor(0, linea * 16 + 2);
  display.println(x);
  display.display();
}

void song(){
    tone(BUZZER, 440);
    delay(1000);
    noTone(BUZZER);
}

void login(){
  HTTPClient http;
    http.begin(URL "login");
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST("{\"username\":\"admin\", \"password\":\"123\"}");
    if(httpCode > 0) {
      if(httpCode == HTTP_CODE_OK) {
          String payload = http.getString();
          Serial.println(payload);
          int p = payload.indexOf("}");
          jwt = payload.substring(22,p-1);
          jwt = buscarJson("jwt", payload);
          Serial.println(jwt);
          sesion = true;
      }
    }
    http.end();
}

void pay(){
  HTTPClient http;
  http.begin(URL "movimientos");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + jwt);
  int httpCode = http.POST("{\"cargo\":10.00,\"tagid\":\"" + String(tagId32.integer) + "\"}");
  if(httpCode > 0) {
      if(httpCode == 201) {
        String payload = http.getString();
        Serial.println(payload);
        Serial.println("Registro exitoso");
        //String msg = buscarJson("cargo", payload);
        //Serial.println(msg);
        //printLCD("PAGO:"+ msg);
        //msg = buscarJson("saldo", payload);
        //Serial.println(msg);
        //printLCD("SALDO:", 1);
        //printLCD(msg, 2);
        printLCD("-10.00 MXN");
        digitalWrite(LED_GREEN, HIGH);
        delay(5000);
        printLCD("BIENVENIDO");
        digitalWrite(LED_GREEN, LOW);
        return;
      }else{
        Serial.println("No se pudo cobrar " + httpCode);
      }
  }else{
    Serial.println("No hay conexion con servidor " + httpCode);
  }
  printLCD("ERROR AL COBRAR");
  digitalWrite(LED_RED, HIGH);
  delay(5000);
  printLCD("BIENVENIDO");
  digitalWrite(LED_RED, LOW);
 }

String buscarJson(String clave, String json){
  int i,f;
  String valor;
  i = json.indexOf(clave);
  i = json.indexOf(":", i);
  i = json.indexOf("\"", i) + 1;
  f = json.indexOf("\"", i);
  return json.substring(i, f);
}
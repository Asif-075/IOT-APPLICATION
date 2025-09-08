#include "MAX30100_PulseOximeter.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h>


#include <WiFiClientSecure.h>
String readString;
const char* host = "script.google.com";
const int httpsPort = 443;
WiFiClientSecure client;
String GAS_ID = "AKfycbzzB1PbmI_cKR1-BcJxeU0LyOA-ixyKtfhVDvDhApnxn0F2EsDb-Jq-x_3PpAvL60CT";

char* esp_ssid = "IoT Project";
char* esp_pass = "1234567890";

//---------------
byte NTCPin = A0;
float Vin = 3.3;   // [V]
float Rt = 10000;  // Resistor t [ohm]
float R0 = 10000;  // value of rct in T0 [ohm]
float T0 = 298.15; // use T0 in Kelvin [K]
float Vout = 0.0;  // Vout in A0
float Rout = 0.0;  // Rout in A0
// use the datasheet to get this data.
float T1 = 273.15;    // [K] in datasheet 0º C
float T2 = 373.15;    // [K] in datasheet 100° C
float RT1 = 35563; // [ohms]  resistence in T1
float RT2 = 549;  // [ohms]   resistence in T2
float beta = 0.0;  // initial parameters [K]
float Rinf = 0.0;  // initial parameters [ohm]
float TempK = 0.0; // variable output
float TempC = 0.0; // variable output

#define G_LED    D6
#define R_LED    D5

#define BUZZER   D7

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define REPORTING_PERIOD_MS 1000

// Connections : SCL PIN - D1 , SDA PIN - D2 , INT PIN - D0
PulseOximeter pox;

float BPM, t, bpm_av = 0, t_av = 0;
int SpO2, spo2_av = 0;
int r = 0, co = 0;
int counterTS = 30;

uint32_t tsLastReport = 0;

const unsigned char bitmap [] PROGMEM =
{
  0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x18, 0x00, 0x0f, 0xe0, 0x7f, 0x00, 0x3f, 0xf9, 0xff, 0xc0,
  0x7f, 0xf9, 0xff, 0xc0, 0x7f, 0xff, 0xff, 0xe0, 0x7f, 0xff, 0xff, 0xe0, 0xff, 0xff, 0xff, 0xf0,
  0xff, 0xf7, 0xff, 0xf0, 0xff, 0xe7, 0xff, 0xf0, 0xff, 0xe7, 0xff, 0xf0, 0x7f, 0xdb, 0xff, 0xe0,
  0x7f, 0x9b, 0xff, 0xe0, 0x00, 0x3b, 0xc0, 0x00, 0x3f, 0xf9, 0x9f, 0xc0, 0x3f, 0xfd, 0xbf, 0xc0,
  0x1f, 0xfd, 0xbf, 0x80, 0x0f, 0xfd, 0x7f, 0x00, 0x07, 0xfe, 0x7e, 0x00, 0x03, 0xfe, 0xfc, 0x00,
  0x01, 0xff, 0xf8, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00, 0x7f, 0xe0, 0x00, 0x00, 0x3f, 0xc0, 0x00,
  0x00, 0x0f, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void onBeatDetected()
{
  Serial.println("Beat Detected!");
  if (BPM > 10 && SpO2 > 10) {
    oled.drawBitmap( 100, 20, bitmap, 28, 28, 1);
    oled.display();
  }
}

const int Erasing_button = 0;

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setup()
{
  beta = (log(RT1 / RT2)) / ((1 / T1) - (1 / T2));
  Rinf = R0 * exp(-beta / T0);
  Serial.begin(9600);
  oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(1);
  oled.setCursor(0, 0);

  oled.println("WiFi connecting...");
  oled.display();

  pinMode(Erasing_button, INPUT);
  pinMode(D4, OUTPUT);
  pinMode(G_LED, OUTPUT);
  pinMode(R_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(G_LED, LOW);
  digitalWrite(R_LED, LOW);
  digitalWrite(BUZZER, LOW);

  for (uint8_t t = 4; t > 0; t--) {
    digitalWrite(D4, LOW);
    delay(500);
    Serial.println(t);
    digitalWrite(D4, HIGH);
    delay(500);
  }

  // Press and hold the button to erase all the credentials
  if (digitalRead(Erasing_button) == LOW)
  {
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    ESP.restart();
    delay(1000);
  }

  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  if (!wifiManager.autoConnect(esp_ssid, esp_pass)) {
    Serial.println("failed to connect and hit timeout");
    ESP.restart();
    delay(1000);
  }
  Serial.println("connected...yeey :)");
  digitalWrite(D4, LOW);
  client.setInsecure();

  Serial.print("Initializing Pulse Oximeter..");

  if (!pox.begin())
  {
    Serial.println("FAILED");
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(1);
    oled.setCursor(0, 0);
    oled.println("FAILED");
    oled.display();
    for (;;);
  }
  else
  {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(1);
    oled.setCursor(0, 0);
    oled.println("SUCCESS");
    oled.display();
    Serial.println("SUCCESS");
    pox.setOnBeatDetectedCallback(onBeatDetected);
  }
}

void loop()
{
  pox.update();

  BPM = pox.getHeartRate();
  SpO2 = pox.getSpO2();

  Vout = Vin * ((float)(analogRead(NTCPin)) / 1024.0); // calc for ntc
  Rout = (Rt * Vout / (Vin - Vout));

  TempK = (beta / log(Rout / Rinf)); // calc for temperature
  TempC = TempK - 273.15;
  TempC = ((TempC * 1.8) + 32);
  t = TempC;

  if (millis() - tsLastReport > REPORTING_PERIOD_MS)
  {
    Serial.print("Heart rate:");
    Serial.print(BPM);
    Serial.print(" bpm / SpO2:");
    Serial.print(SpO2);
    Serial.println(" %");

    if (BPM <= 10 || SpO2 <= 10) {
      oled.clearDisplay();
      oled.setTextSize(2);
      oled.setTextColor(1);
      oled.setCursor(30, 0);
      oled.println("Please");
      oled.setCursor(35, 20);
      oled.println("Press");
      oled.setCursor(30, 40);
      oled.println("Finger");
      oled.display();
      BPM = 00.00;
      SpO2 = 00.00;
      t = 00.00;
      co = 0;
      bpm_av = 0;
      spo2_av = 0;
      t_av = 0;
    }
    else {
      oled.clearDisplay();
      oled.setTextSize(1);
      oled.setTextColor(1);

      int counterT = (counterTS - co);
      Serial.println(counterT);
      oled.setCursor(0, 0);
      if (counterT < 10)
        oled.print("Please Wait 0");
      else
        oled.print("Please Wait ");
      oled.print(counterT);
      oled.println(" S");

      oled.setCursor(0, 16);
      oled.print("BPM :");
      oled.println(BPM);

      oled.setCursor(0, 30);
      oled.print("Spo2:");
      oled.println(SpO2);

      oled.setCursor(0, 45);
      oled.print("Temp:");
      oled.println(t);
      oled.display();

      if (co == counterTS) {
        BPM = bpm_av / counterTS;
        SpO2 = spo2_av / counterTS;
        t = t_av / counterTS;
        if ((t <= 98.40) && (t >= 97.00) && (SpO2 <= 100) && (SpO2 >= 95)  && (BPM <= 100) && (BPM >= 60)) {
          digitalWrite(G_LED, HIGH);
          digitalWrite(R_LED, LOW);
          digitalWrite(BUZZER, LOW);
          oled.clearDisplay();
          oled.setTextSize(1);
          oled.setTextColor(1);
          oled.setCursor(0, 0);
          oled.println("Normal!");

          oled.setCursor(0, 16);
          oled.print("BPM :");
          oled.println(BPM);

          oled.setCursor(0, 30);
          oled.print("Spo2:");
          oled.println(SpO2);

          oled.setCursor(0, 45);
          oled.print("Temp:");
          oled.println(t);
          oled.display();
        }
        else {
          digitalWrite(G_LED, LOW);
          digitalWrite(R_LED, HIGH);
          digitalWrite(BUZZER, HIGH);
          oled.clearDisplay();
          oled.setTextSize(1);
          oled.setTextColor(1);
          oled.setCursor(0, 0);
          oled.println("Contact With Doctor.");

          oled.setCursor(0, 16);
          oled.print("BPM :");
          oled.println(BPM);

          oled.setCursor(0, 30);
          oled.print("Spo2:");
          oled.println(SpO2);

          oled.setCursor(0, 45);
          oled.print("Temp:");
          oled.println(t);
          if (t > 99) {
            oled.setCursor(0, 55);
            oled.print("Paracetamol 500-650mg");
          }
          oled.display();
        }
        writing();
        delay(3000);
        pox.begin();
        digitalWrite(G_LED, LOW);
        digitalWrite(R_LED, LOW);
        digitalWrite(BUZZER, LOW);
      }
      bpm_av = bpm_av + BPM;
      spo2_av = spo2_av + SpO2;
      t_av = t_av + t;
    }
    tsLastReport = millis();
    co++;
  }
}

void writing() {
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  String url = "/macros/s/" + GAS_ID + "/exec?value1=" + String(BPM) + "&value2=" + String(SpO2) + "&value3=" + String(t);
  Serial.print("requesting URL: ");
  Serial.println(url);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");
  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");
}

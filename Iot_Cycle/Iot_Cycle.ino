#include <ESP8266WiFi.h>
#include <ThingerESP8266.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define USERNAME "priyanshusaini"
#define DEVICE_ID "esp8266_IoT_cycle"
#define DEVICE_CREDENTIAL "*********" 

#define WIFI_SSID "void"
#define WIFI_PASSWORD "*********"

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define GPS_BAUD 9600
TinyGPSPlus gps;
SoftwareSerial gpsSerial(12, 14);

ThingerESP8266 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);

const unsigned char heart_bmp[] PROGMEM = {
  0x00, 0x00, 0x18, 0x18, 0x3c, 0x3c, 0x7e, 0x7e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
  0x7e, 0x7e, 0x3c, 0x3c, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const int sensorPin = 13; 

volatile unsigned long lastPulse = 0;
volatile unsigned long pulseInterval = 0;
volatile double distance = 0;
volatile bool newPulse = false;

double latitude = 0;
double longitude = 0;
double speed = 0;
double rpm = 0;

bool email_sent = false;

int animFrame = 0;

unsigned long lastDisplayUpdate = 0;
const long displayInterval = 250;

unsigned long displayStartTime = 0;     
const long displayDuration = 5000;      
bool isDisplayActive = false;            
void IRAM_ATTR onPulse() {

    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  
    unsigned long now = micros();
    unsigned long delta = now - lastPulse;

    if (delta > 200000UL) { 
        pulseInterval = delta;
        lastPulse = now;
        distance += 0.00215;
        newPulse = true;
    }
}

void setup() {
    Serial.begin(9600);
    gpsSerial.begin(GPS_BAUD);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(sensorPin, INPUT_PULLUP); 
    attachInterrupt(digitalPinToInterrupt(sensorPin), onPulse, RISING);

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); 
    }
    
    display.display();
    delay(3000);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Booting Cycle IoT...");
    display.display();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    delay(3000);

    thing["nudge"] << [](pson& in) {
        if(in.is_empty() || (bool)in == true) { 
            displayStartTime = millis();
            isDisplayActive = true;
        }
    };

    thing["cycle"] >> [](pson &out){
        out["lat"] = latitude;
        out["lng"] = longitude;
        out["speed"] = speed;
        out["dist"] = distance;
    };
}

void loop() {
    
    if (WiFi.status() == WL_CONNECTED) {
        thing.handle();
        
        if (!email_sent) {
            email_sent = thing.call_endpoint("email");
            Serial.print(F("Email attempt made - ")); Serial.println(email_sent); 
        }
    } 
    else {

        static unsigned long lastAttempt = 0;
        if (millis() - lastAttempt > 30000) {
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        lastAttempt = millis();
        }
    }

    while (gpsSerial.available()) {
        gps.encode(gpsSerial.read());
    }

    if (gps.location.isValid() && gps.location.isUpdated()) {
        latitude = gps.location.lat();
        longitude = gps.location.lng();
    }

    unsigned long currentMicros = micros();
    
    if (currentMicros - lastPulse > 4000000UL) {
        rpm = 0.0;
        speed = 0.0;
    } else if (pulseInterval > 0) {

        rpm = 60.0e6 / pulseInterval;             
        speed = 2.15 * rpm * 60.0 / 1000.0; // km/h
    }

    // if (millis() - lastDisplayUpdate >= displayInterval) {
    //     lastDisplayUpdate = millis();
    //     updateDisplay();
    // }

    if (isDisplayActive && (millis() - displayStartTime >= displayDuration)) {
        isDisplayActive = false;
    }


    if (millis() - lastDisplayUpdate >= displayInterval) {
            lastDisplayUpdate = millis();
            updateDisplay();
    }
    
}

void updateDisplay() {
    display.clearDisplay(); 

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println(F("--- IoT CYCLE ---"));


    display.setCursor(0, 16);
    display.print(F("SPD: "));
    display.setTextSize(2);
    display.print(speed, 1); 
    display.setTextSize(1);
    display.print(F(" km/h"));

    display.setCursor(0, 40);
    display.print(F("DST: "));
    display.print(distance, 2); 
    display.print(F(" km"));
    

    if (WiFi.status() == WL_CONNECTED) {
        display.drawRect(118, 6, 2, 2, WHITE); 
        display.drawRect(121, 4, 2, 4, WHITE); 
        display.drawRect(124, 2, 2, 6, WHITE); 
    } else {
        display.drawLine(120, 2, 126, 8, WHITE); 
        display.drawLine(126, 2, 120, 8, WHITE); 
    }

    if (gps.location.isValid()) {
       display.drawCircle(108, 5, 3, WHITE); 
       display.drawPixel(108, 5, WHITE);
    } else {
       display.drawCircle(108, 5, 1, WHITE);
    }


    long roadPosition = (long)(distance * 10000); 
    int roadOffset = roadPosition % 16;

    for (int x = -16; x < 128; x += 16) {
        display.drawLine(x + roadOffset, 63, x + roadOffset + 8, 63, WHITE);
    }
            
    if (isDisplayActive) {
        int beat = (millis() / 250) % 2; 
        int hX = 110; 
        int hY = 25;  
        
        if (beat == 0) {
            display.drawBitmap(hX, hY, heart_bmp, 16, 16, WHITE);
        } else {
            display.fillCircle(hX + 4, hY + 4, 3, WHITE); 
            display.fillCircle(hX + 12, hY + 4, 3, WHITE); 
            display.fillTriangle(hX + 1, hY + 5, hX + 15, hY + 5, hX + 8, hY + 14, WHITE); 
        }
    }


    display.display();
}

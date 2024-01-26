#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
//#include <U8g2lib.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <HX711.h>
//#include <MFRC522.h>
#include <SPI.h>

// Wi-Fi Configuration
const char* ssid = "insertssid";
const char* password = "insertpw";

// API settings
const char* serverName = "http://insert-spoolman-IP-and-Port/api/v1";
unsigned long updateInterval = 60000;
unsigned long lastUpdateTime = 0;
String jsonCache;

// OLED settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SSD1306_I2C_ADDRESS 0x3C
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// Rotary Encoder settings
#define ROTARY_CLK 32
#define ROTARY_DT 25
#define ROTARY_SW 27

// HX711 settings
#define DOUT  16
#define CLK  4
HX711 scale;

// RC522 settings
//#define RST_PIN  26
//#define SS_PIN  5
//MFRC522 rfid(SS_PIN, RST_PIN);


// Buzzer settings
#define BUZZER_PIN  0

int currentStateCLK;
int lastStateCLK;
String currentMenu = "main";
int selected = 0;
int firstItemIndex = 0; // Added this for scrolling menu

void displayMenu(DynamicJsonDocument& doc, String menu, int selected);
void performAction(String name, String vendor, int id);
void updateWeight(int filamentPosition);
void updateFilamentWeight(int filamentPosition, float weight);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting setup...");
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.setRotation(2); //rotates text on OLED 1=90 degrees, 2=180 degrees
  display.display();
  delay(2000);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Hello OLED");
  display.display();
  delay(2000);

  // Initialize HX711
  Serial.println(F("Scale begin"));
  scale.begin(DOUT, CLK);
  scale.set_scale(1235.97212); // Adjust this value based on your calibration
  long zero_factor = scale.read_average();
  scale.tare();

  // Initialize RC522 RFID reader
  //Serial.println(F("SPI begin"));
  //SPI.begin();
  //rfid.PCD_Init();

   // Rotary Encoder settings
  pinMode(ROTARY_CLK, INPUT);
  pinMode(ROTARY_DT, INPUT);
  pinMode(ROTARY_SW, INPUT_PULLUP);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);

  currentStateCLK = digitalRead(ROTARY_CLK);
  lastStateCLK = currentStateCLK;

  Serial.println("Setup complete.");
}

void beepbuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);   // turn the Buzzer on (HIGH is the voltage level)
  delay(300);                       // wait for a second
  digitalWrite(BUZZER_PIN, LOW);    // turn the Buzzer off by making the voltage LOW
  delay(100);                       // wait for a second
}

String fetchFilamentList() {
  unsigned long currentTime = millis();
  if (currentTime - lastUpdateTime < updateInterval && lastUpdateTime != 0) {
    Serial.println("Using cached JSON.");
    return jsonCache;
  }
  Serial.println("Must update JSON.");
  HTTPClient http;
  String geturl = String(serverName) + "/spool";
  http.begin(geturl);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
    http.end();
    lastUpdateTime = currentTime;
    jsonCache = payload;
    return payload;
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
    http.end();
  }
  return "";
}

void updateWeight(int filamentID) {
  float weight = scale.get_units(20);  // Get weight from HX711
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Weight: ");
  display.println(weight);
  display.display();
  delay(2000);
  updateFilamentWeight(filamentID, weight);
}

void updateFilamentWeight(int filamentID, float weight) {
  HTTPClient http;
  http.begin(String(serverName) + "/spool/" + String(filamentID));
  http.addHeader("Content-Type", "application/json");
  String requestData = "{\"remaining_weight\": " + String(weight) + "}";
  int httpCode = http.PATCH(requestData);
  http.end();
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Spoolman Updated!");
  display.display();
  delay(2000);
}

void loop() {
  Serial.println("Entering loop...");
  String jsonPayload = fetchFilamentList();
  DynamicJsonDocument doc(4096);
  deserializeJson(doc, jsonPayload);
  if (doc.isNull()) {
    Serial.println("Failed to deserialize JSON.");
    return;
  }

  displayMenu(doc, currentMenu, selected);
  
  while (true) {
    currentStateCLK = digitalRead(ROTARY_CLK);
    int menuSize = doc.size();

    if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
      if (digitalRead(ROTARY_DT) != currentStateCLK) {
        selected = (selected + 1) % menuSize;
        if (selected >= firstItemIndex + 6) {
          firstItemIndex++;  // Increase first item index for scrolling down
        }
      } else {
        selected = (selected - 1 + menuSize) % menuSize;
        if (selected < firstItemIndex) {
          firstItemIndex--;  // Decrease first item index for scrolling up
        }
      }
      displayMenu(doc, currentMenu, selected);
    }
    lastStateCLK = currentStateCLK;

    if (digitalRead(ROTARY_SW) == LOW) {
      delay(20);
      while (digitalRead(ROTARY_SW) == LOW);
      delay(20);
      if (selected < doc.size()) {
        JsonObject menuItem = doc[selected].as<JsonObject>();
        String action = menuItem["action"].as<String>();
        if (action == "submenu") {
          currentMenu = menuItem["submenu"].as<String>();
          selected = 0;
          firstItemIndex = 0;  // Reset first item index when entering submenu
        } else {
          String name = menuItem["filament"]["name"];
          String vendor = menuItem["filament"]["vendor"]["name"];
          int id = menuItem["id"];
          performAction(name, vendor, id);
        }
      } else if (currentMenu != "main") {
        currentMenu = "main";
        selected = 0;
        firstItemIndex = 0;  // Reset first item index when returning to main menu
      }
      displayMenu(doc, currentMenu, selected);
    }
  }
}

void displayMenu(DynamicJsonDocument& doc, String menu, int selected) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int y = 0;
  int index = firstItemIndex;  // Start from first item index
  for (int i = 0; i < 6; i++) {  // Only display 6 items at most
    if (index >= doc.size()) break;  // Break if reached the end
    JsonObject menuItem = doc[index].as<JsonObject>();
    if (index == selected) {
      display.fillRect(0, y * 10, SCREEN_WIDTH, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0, y * 10);
    display.print(menuItem["id"].as<String>());
    display.setCursor(20, y * 10);
    display.print(menuItem["filament"]["name"].as<String>());
    
    y++;
    index++;
  }
  display.display();
}

void performAction(String name, String vendor, int id) {
  beepbuzzer();
  Serial.println("Performing update Spoolman for:");
  Serial.println("Name: " + name);
  Serial.println("Vendor: " + vendor);
  Serial.println("ID: " + String(id));
  updateWeight(id);
}

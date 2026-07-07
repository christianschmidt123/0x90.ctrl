#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Update.h>
#include <vector>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"

#define NO_PIN -1
const int CURRENT_VERSION = 1;

enum ActionType { TYPE_KEYBOARD_MACRO, TYPE_CONSUMER_CONTROL };

struct MacroDefinition {
  String id;
  String name;
  ActionType type;
  std::vector<uint16_t> keys;
  int gpioPin;
  bool wasPressed;
  unsigned long lastRepeat;
  String icon;
  int layer;
};

std::vector<MacroDefinition> myMacros;
int activeLayer = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
DNSServer dnsServer;
Preferences preferences;
USBHIDKeyboard Keyboard;
USBHIDConsumerControl ConsumerControl;

bool isAPMode = false;
String ssidStore, passStore;

bool shouldRestart = false;
unsigned long restartMillis = 0;

// --- DYNAMISCHER TASTEN-PARSER FÜR BELIEBIG TIEFE VERSCHACHTELUNG ---
std::vector<uint16_t> parseKeys(String keyStr, ActionType type) {
  std::vector<uint16_t> keys;
  if (type == TYPE_CONSUMER_CONTROL) {
    if (keyStr == "MUTE") keys.push_back(0xE2);
    else if (keyStr == "VOL_UP") keys.push_back(0xE9);
    else if (keyStr == "VOL_DOWN") keys.push_back(0xEA);
  } else {
    int startIndex = 0;
    int endIndex = keyStr.indexOf('+');
    
    while (startIndex < keyStr.length()) {
      String token;
      if (endIndex == -1) {
        token = keyStr.substring(startIndex);
        startIndex = keyStr.length();
      } else {
        token = keyStr.substring(startIndex, endIndex);
        startIndex = endIndex + 1;
        endIndex = keyStr.indexOf('+', startIndex);
      }
      
      token.trim();
      if (token.length() == 0) continue;

      // Modifikatoren
      if (token == "CTRL") keys.push_back(KEY_LEFT_CTRL);
      else if (token == "SHIFT") keys.push_back(KEY_LEFT_SHIFT);
      else if (token == "ALT") keys.push_back(KEY_LEFT_ALT);
      
      // Standard-Sondertasten
      else if (token == "ENTER") keys.push_back(KEY_RETURN);
      else if (token == "SPACE") keys.push_back(' ');
      else if (token == "BACKSPACE") keys.push_back(KEY_BACKSPACE);
      else if (token == "ESC") keys.push_back(KEY_ESC);
      
      // Navigation & Editieren
      else if (token == "DELETE") keys.push_back(KEY_DELETE);
      else if (token == "INSERT") keys.push_back(KEY_INSERT);
      else if (token == "HOME") keys.push_back(KEY_HOME);
      else if (token == "END") keys.push_back(KEY_END);
      else if (token == "PAGE_UP") keys.push_back(KEY_PAGE_UP);
      else if (token == "PAGE_DOWN") keys.push_back(KEY_PAGE_DOWN);
      
      // Pfeiltasten
      else if (token == "UP") keys.push_back(KEY_UP_ARROW);
      else if (token == "DOWN") keys.push_back(KEY_DOWN_ARROW);
      else if (token == "LEFT") keys.push_back(KEY_LEFT_ARROW);
      else if (token == "RIGHT") keys.push_back(KEY_RIGHT_ARROW);
      
      // Funktionstasten
      else if (token == "F1") keys.push_back(KEY_F1);
      else if (token == "F2") keys.push_back(KEY_F2);
      else if (token == "F3") keys.push_back(KEY_F3);
      else if (token == "F4") keys.push_back(KEY_F4);
      else if (token == "F5") keys.push_back(KEY_F5);
      else if (token == "F6") keys.push_back(KEY_F6);
      else if (token == "F7") keys.push_back(KEY_F7);
      else if (token == "F8") keys.push_back(KEY_F8);
      else if (token == "F9") keys.push_back(KEY_F9);
      else if (token == "F10") keys.push_back(KEY_F10);
      else if (token == "F11") keys.push_back(KEY_F11);
      else if (token == "F12") keys.push_back(KEY_F12);
      
      // Einzelbuchstaben & Zahlen
      else if (token.length() == 1) keys.push_back(token.charAt(0));
    }
  }
  return keys;
}

void loadConfig() {
  if (!LittleFS.exists("/config.json")) {
    myMacros = {
      {"mute_id", "Audio Mute", TYPE_CONSUMER_CONTROL, {0xE2}, 4, false, 0, "bi-volume-mute-fill", 0},
      {"volup_id", "Lauter", TYPE_CONSUMER_CONTROL, {0xE9}, 5, false, 0, "bi-volume-up-fill", 0},
      {"voldown_id", "Leiser", TYPE_CONSUMER_CONTROL, {0xEA}, 6, false, 0, "bi-volume-down-fill", 0}
    };
    return;
  }
  File file = LittleFS.open("/config.json", "r");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) { Serial.println("Fehler beim Parsen der config.json!"); return; }

  myMacros.clear();
  JsonArray arr = doc["macros"].as<JsonArray>();
  for (JsonObject obj : arr) {
    MacroDefinition m;
    m.id = obj["id"].as<String>(); m.name = obj["name"].as<String>();
    m.type = (ActionType)obj["type"].as<int>(); m.gpioPin = obj["gpio"].as<int>();
    m.icon = obj["icon"].as<String>(); m.layer = obj["layer"].as<int>();
    m.wasPressed = false; m.lastRepeat = 0;
    m.keys = parseKeys(obj["keys"].as<String>(), m.type);
    myMacros.push_back(m);
  }
}

void executeMacro(const MacroDefinition& macro) {
  if (macro.type == TYPE_CONSUMER_CONTROL) {
    for (uint16_t key : macro.keys) { ConsumerControl.press(key); }
    delay(10); ConsumerControl.release();
  } else {
    for (uint16_t key : macro.keys) { Keyboard.press(key); }
    delay(50); Keyboard.releaseAll();
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) { client->text("LAYER_CHANGED:" + String(activeLayer)); }
  else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0; String message = (char*)data;
      if (message.startsWith("TRIGGER:")) {
        String searchId = message.substring(8);
        for (const auto& macro : myMacros) {
          if (macro.id == searchId && macro.layer == activeLayer) { executeMacro(macro); ws.textAll("ACK:" + searchId); return; }
        }
      } else if (message.startsWith("SET_LAYER:")) {
        activeLayer = message.substring(10).toInt(); ws.textAll("LAYER_CHANGED:" + String(activeLayer));
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if(!LittleFS.begin(true)) { Serial.println("LittleFS Mount Failed!"); }
  loadConfig();

  for (const auto& macro : myMacros) { if (macro.gpioPin != NO_PIN) pinMode(macro.gpioPin, INPUT_PULLUP); }
  Keyboard.begin(); ConsumerControl.begin(); USB.begin();

  preferences.begin("wifi-config", false);
  ssidStore = preferences.getString("ssid", ""); 
  passStore = preferences.getString("pass", "");

  WiFi.mode(WIFI_STA);
  if (ssidStore != "") {
    WiFi.begin(ssidStore.c_str(), passStore.c_str());
    int c = 0; while (WiFi.status() != WL_CONNECTED && c < 15) { delay(500); c++; }
  }

  if (WiFi.status() != WL_CONNECTED) {
    isAPMode = true; 
    int numNetworks = WiFi.scanNetworks();
    WiFi.disconnect(); WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("ESP32-MacroPad-Setup");
    delay(500); dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    server.on("/", HTTP_GET, [numNetworks](AsyncWebServerRequest *request){
       request->send(200, "text/html", "<h1>WLAN Setup Modus aktiv</h1>");
    });
    server.onNotFound([](AsyncWebServerRequest *request){ request->redirect("http://192.168.4.1/"); });
  } else {
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    // Hauptseite (Index)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      if(LittleFS.exists("/index.html")) request->send(LittleFS, "/index.html", "text/html");
      else request->send(200, "text/plain", "index.html fehlt!");
    });

    // --- VISUELLER HOTKEY- & ICON-EDITOR ---
    server.on("/editor", HTTP_GET, [](AsyncWebServerRequest *request){
      if(LittleFS.exists("/editor.html")) {
        request->send(LittleFS, "/editor.html", "text/html");
      } else {
        request->send(404, "text/plain", "editor.html fehlt im LittleFS!");
      }
    });

    // --- WEB-IDE ANZEIGE ---
    server.on("/ide", HTTP_GET, [](AsyncWebServerRequest *request){
      if(LittleFS.exists("/ide.html")) {
        request->send(LittleFS, "/ide.html", "text/html");
      } else {
        request->send(404, "text/plain", "ide.html fehlt im LittleFS!");
      }
    });

    // API für Dateibrowser der IDE (Listet alle Dateien auf)
    server.on("/api/listfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
      String json = "{\"files\":[";
      File root = LittleFS.open("/");
      File file = root.openNextFile();
      while(file) {
        if(json != "{\"files\":[") json += ",";
        json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
        file = root.openNextFile();
      }
      json += "]}";
      request->send(200, "application/json", json);
    });

    // --- OTA UPDATER ---
    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
      if(LittleFS.exists("/update.html")) {
        request->send(LittleFS, "/update.html", "text/html");
      } else {
        request->send(404, "text/plain", "update.html fehlt im LittleFS!");
      }
    });

    // POST-Empfänger für Firmware-Flash (.bin)
    server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
      shouldRestart = !Update.hasError();
      restartMillis = millis() + 2000;
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldRestart ? "Update erfolgreich! Startet neu..." : "Update fehlgeschlagen!");
      response->addHeader("Connection", "close");
      request->send(response);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (!index) {
        Serial.printf("Update Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        }
      }
      if (!Update.hasError()) {
        if (Update.write(data, len) != len) {
          Update.printError(Serial);
        }
      }
      if (final) {
        if (Update.end(true)) {
          Serial.printf("Update erfolgreich: %u Bytes\n", index + len);
        } else {
          Update.printError(Serial);
        }
      }
    });

    // --- SPEICHER-APIs (Makro-Konfiguration) ---
    server.on("/api/macros", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/config.json", "application/json");
    });

    server.on("/api/macros/save", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      static String buffer = "";
      if (index == 0) buffer = "";
      for (size_t i = 0; i < len; i++) { buffer += (char)data[i]; }
      
      if (index + len == total) {
        File f = LittleFS.open("/config.json", "w");
        if (f) {
          f.print(buffer); f.close();
          loadConfig(); 
          request->send(200, "text/plain", "OK");
        } else { request->send(500); }
      }
    });

    // --- UNIVERSAL FILE SAVE API (Für die Web-IDE) ---
    server.on("/api/uploadfile", HTTP_POST, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", "Datei erfolgreich gespeichert");
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      static File uploadFile;
      if (index == 0) {
        String filename = "/";
        if (request->hasHeader("X-Filename")) {
          filename += request->getHeader("X-Filename")->value();
        } else if (request->hasParam("file")) {
          filename += request->getParam("file")->value();
        } else {
          filename += "temp.txt";
        }
        uploadFile = LittleFS.open(filename, "w");
      }
      if (uploadFile) { uploadFile.write(data, len); }
      if (index + len == total) {
        if (uploadFile) {
          String fname = uploadFile.name();
          uploadFile.close();
          if (fname.endsWith("config.json")) { loadConfig(); }
        }
      }
    });

    // --- DIE WICHTIGSTE ERWEITERUNG: STATISCHER DATEI-SERVER ---
    // Erlaubt das direkte Herunterladen/Ansehen jeder Datei im LittleFS-Root (z.B. /editor.html, /config.json)
    server.serveStatic("/", LittleFS, "/");
  }
  server.begin();
}

void loop() {
  if (shouldRestart && millis() > restartMillis) { ESP.restart(); }
  if (isAPMode) { dnsServer.processNextRequest(); delay(10); return; }
  ws.cleanupClients();
  
  for (auto& macro : myMacros) {
    if (macro.gpioPin != NO_PIN && macro.layer == activeLayer) {
      bool isPressed = (digitalRead(macro.gpioPin) == LOW);
      if (isPressed && !macro.wasPressed) {
        executeMacro(macro); macro.wasPressed = true; ws.textAll("ACK:" + macro.id); delay(100);
      } else if (!isPressed && macro.wasPressed) { macro.wasPressed = false; }
    }
  }
  delay(1);
}
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

enum ActionType { TYPE_HOTKEY = 0, TYPE_CONSUMER_CONTROL = 1, TYPE_TEXT_SEQUENCE = 2 };

struct MacroDefinition {
  String id;
  String name;
  ActionType type;
  String rawKeysString; 
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

// --- DEBUG WEB LOG SYSTEM ---
std::vector<String> debugLog;
const size_t MAX_LOG_LINES = 50;

void logToWeb(String message) {
  unsigned long timestamp = millis();
  String line = "[" + String(timestamp) + "] " + message;
  Serial.println(line);
  debugLog.push_back(line);
  if (debugLog.size() > MAX_LOG_LINES) {
    debugLog.erase(debugLog.begin());
  }
}

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

      if (token == "CTRL") keys.push_back(KEY_LEFT_CTRL);
      else if (token == "SHIFT") keys.push_back(KEY_LEFT_SHIFT);
      else if (token == "ALT") keys.push_back(KEY_LEFT_ALT);
      else if (token == "ENTER") keys.push_back(KEY_RETURN);
      else if (token == "SPACE") keys.push_back(' ');
      else if (token == "BACKSPACE") keys.push_back(KEY_BACKSPACE);
      else if (token == "ESC") keys.push_back(KEY_ESC);
      else if (token == "DELETE") keys.push_back(KEY_DELETE);
      else if (token == "INSERT") keys.push_back(KEY_INSERT);
      else if (token == "HOME") keys.push_back(KEY_HOME);
      else if (token == "END") keys.push_back(KEY_END);
      else if (token == "PAGE_UP") keys.push_back(KEY_PAGE_UP);
      else if (token == "PAGE_DOWN") keys.push_back(KEY_PAGE_DOWN);
      else if (token == "UP") keys.push_back(KEY_UP_ARROW);
      else if (token == "DOWN") keys.push_back(KEY_DOWN_ARROW);
      else if (token == "LEFT") keys.push_back(KEY_LEFT_ARROW);
      else if (token == "RIGHT") keys.push_back(KEY_RIGHT_ARROW);
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
      else if (token.length() == 1) {
        keys.push_back((uint16_t)token.charAt(0)); 
      }
    }
  }
  return keys;
}

void loadConfig() {
  if (!LittleFS.exists("/config.json")) {
    logToWeb("WARNUNG: /config.json existiert nicht!");
    return;
  }
  File file = LittleFS.open("/config.json", "r");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    logToWeb("FEHLER beim Parsen der config.json: " + String(error.c_str()));
    return;
  }

  myMacros.clear();
  JsonArray arr = doc["macros"].as<JsonArray>();
  for (JsonObject obj : arr) {
    MacroDefinition m;
    m.id = obj["id"].as<String>(); m.name = obj["name"].as<String>();
    m.type = (ActionType)obj["type"].as<int>(); m.gpioPin = obj["gpio"].as<int>();
    m.icon = obj["icon"].as<String>(); m.layer = obj["layer"].as<int>();
    m.wasPressed = false; m.lastRepeat = 0;
    
    String rawKeys = obj["keys"].as<String>();
    m.rawKeysString = rawKeys;
    m.keys = parseKeys(rawKeys, m.type);
    myMacros.push_back(m);
  }
  logToWeb("Konfiguration erfolgreich geladen. Makros geladen: " + String(myMacros.size()));
}

void executeMacro(const MacroDefinition& macro) {
  logToWeb("Fuehre Makro aus: '" + macro.name + "' (Typ: " + String(macro.type) + ", Keys: " + macro.rawKeysString + ")");
  
  if (macro.type == TYPE_CONSUMER_CONTROL) {
    for (uint16_t key : macro.keys) { ConsumerControl.press(key); }
    delay(10); 
    ConsumerControl.release();
  } 
  else if (macro.type == TYPE_TEXT_SEQUENCE) {
    for (uint16_t key : macro.keys) {
      Keyboard.press(key);
      delay(15); 
      Keyboard.release(key);
      delay(10); 
    }
  } 
  else {
    for (uint16_t key : macro.keys) { Keyboard.press(key); }
    delay(30); 
    Keyboard.releaseAll();
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
          if (macro.id == searchId && macro.layer == activeLayer) { 
            executeMacro(macro); 
            ws.textAll("ACK:" + searchId); 
            return; 
          }
        }
      } else if (message.startsWith("SET_LAYER:")) {
        activeLayer = message.substring(10).toInt(); 
        ws.textAll("LAYER_CHANGED:" + String(activeLayer));
        logToWeb("Layer gewechselt auf: " + String(activeLayer));
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  if(!LittleFS.begin(true)) { Serial.println("LittleFS Mount Failed!"); }
  loadConfig();

  for (const auto& macro : myMacros) { 
    if (macro.gpioPin != NO_PIN && macro.gpioPin > 0) {
      pinMode(macro.gpioPin, INPUT_PULLUP); 
    }
  }
  
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
    WiFi.disconnect(); WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("0x90-deck-Setup");
    delay(500); dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
       request->send(200, "text/html", "<h1>0x90.deck - WLAN Setup Modus</h1>");
    });
    server.onNotFound([](AsyncWebServerRequest *request){ request->redirect("http://192.168.4.1/"); });
  } else {
    ws.onEvent(onEvent);
    server.addHandler(&ws);

    server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request) {
      String out = "<!DOCTYPE html><html><head><title>ESP32 Debug Log</title>";
      out += "<meta http-equiv='refresh' content='2'>"; 
      out += "<style>body{background:#111;color:#0f0;font-family:monospace;padding:20px;} h2{color:#fff;}</style></head><body>";
      out += "<h2>0x90-deck Live Web-Log</h2><hr>";
      if(debugLog.empty()) {
        out += "<p style='color:#666;'>Noch keine Log-Eintraege vorhanden.</p>";
      } else {
        for(int i = debugLog.size() - 1; i >= 0; i--) {
          out += "<div>" + debugLog[i] + "</div>";
        }
      }
      out += "</body></html>";
      request->send(200, "text/html", out);
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      if(LittleFS.exists("/index.html")) request->send(LittleFS, "/index.html", "text/html");
      else request->send(200, "text/plain", "index.html fehlt!");
    });

    server.on("/editor", HTTP_GET, [](AsyncWebServerRequest *request){
      if(LittleFS.exists("/editor.html")) request->send(LittleFS, "/editor.html", "text/html");
      else request->send(404, "text/plain", "editor.html fehlt im LittleFS!");
    });

    server.on("/ide", HTTP_GET, [](AsyncWebServerRequest *request){
      if(LittleFS.exists("/ide.html")) request->send(LittleFS, "/ide.html", "text/html");
      else request->send(404, "text/plain", "ide.html fehlt im LittleFS!");
    });

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

    // KORRIGIERTER PARSER (Eindeutige Typenzuweisung für getParam)
    server.on("/api/macros/save", HTTP_POST, 
      [](AsyncWebServerRequest *request) {
        logToWeb("POST-Anfrage erhalten.");
        
        int params = request->params();
        if(params > 0) {
          logToWeb("Formular-Parameter gefunden: " + String(params));
          
          String targetFile = "/config.json";
          String fileContent = "";
          
          // Eindeutiger Durchlauf aller ankommenden Parameter
          for(int i=0; i<params; i++) {
            const AsyncWebParameter* param = request->getParam((size_t)i); // FIX: Expliziter Cast auf size_t verhindert Ambiguität
            String pName = param->name();
            
            // Methode 1: Die IDE schickt rohes JSON, das fälschlicherweise im Parameternamen landet
            if(i == 0 && (pName.startsWith("{") || pName.startsWith("["))) {
              logToWeb("Erkenne JSON im ersten Parameter-Namen. Sichern...");
              File f = LittleFS.open("/config.json", "w");
              if(f) {
                f.print(pName);
                f.close();
                logToWeb("config.json via Param-Name ueberschrieben.");
                loadConfig();
                request->send(200, "text/plain", "OK");
                return;
              }
            }
            
            // Methode 2: Normales Schlüssel-Wert-Paar-Formular
            logToWeb("Param '" + pName + "' analysiert.");
            if(pName == "path" || pName == "file" || pName == "filename") targetFile = param->value();
            if(pName == "code" || pName == "text" || pName == "content" || pName == "macros" || pName == "data") fileContent = param->value();
          }
          
          if(fileContent.length() > 0) {
            if(!targetFile.startsWith("/")) targetFile = "/" + targetFile;
            File f = LittleFS.open(targetFile, "w");
            if(f) { 
              f.print(fileContent); 
              f.close(); 
              logToWeb(targetFile + " erfolgreich via Form-Param ueberschrieben."); 
              if(targetFile.endsWith("config.json")) loadConfig(); 
            }
            request->send(200, "text/plain", "OK");
            return;
          }
        }
        request->send(200, "text/plain", "Verarbeitet");
      },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        static File multipartFile;
        if (index == 0) {
          if (!filename.startsWith("/")) filename = "/" + filename;
          logToWeb("Starte Multipart-Speicherung: " + filename);
          multipartFile = LittleFS.open(filename, "w");
        }
        if (multipartFile) multipartFile.write(data, len);
        if (final && multipartFile) {
          String name = multipartFile.name();
          multipartFile.close();
          logToWeb("Multipart-Speicherung fertig: " + name);
          if (name.endsWith("config.json")) loadConfig();
        }
      },
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        static File rawFile;
        if (index == 0) {
          String filename = "/config.json";
          if (request->hasHeader("X-Filename")) {
            filename = request->getHeader("X-Filename")->value();
          }
          if (!filename.startsWith("/")) filename = "/" + filename;
          logToWeb("Starte Raw-Body-Speicherung: " + filename + " (" + String(total) + " Bytes)");
          rawFile = LittleFS.open(filename, "w");
        }
        if (rawFile) rawFile.write(data, len);
        if (index + len == total && rawFile) {
          String name = rawFile.name();
          rawFile.close();
          logToWeb("Raw-Body-Speicherung fertig: " + name);
          if (name.endsWith("config.json")) loadConfig();
        }
      }
    );

    server.on("/api/macros", HTTP_GET, [](AsyncWebServerRequest *request){
      if (LittleFS.exists("/config.json")) {
        request->send(LittleFS, "/config.json", "application/json");
      } else {
        request->send(404, "text/plain", "Config fehlt!");
      }
    });

    server.serveStatic("/", LittleFS, "/").setFilter([](AsyncWebServerRequest *request) {
      if (request->method() != HTTP_GET) return false; 
      String path = request->url();
      if (path == "/") path = "/index.html";
      return LittleFS.exists(path);
    });

    server.onNotFound([](AsyncWebServerRequest *request){
      request->send(404, "text/plain", "404: Nicht gefunden!");
    });
  }
  server.begin();
}

void loop() {
  if (shouldRestart && millis() > restartMillis) { ESP.restart(); }
  if (isAPMode) { dnsServer.processNextRequest(); delay(10); return; }
  ws.cleanupClients();
  
  for (auto& macro : myMacros) {
    if (macro.gpioPin != NO_PIN && macro.gpioPin > 0 && macro.layer == activeLayer) {
      bool isPressed = (digitalRead(macro.gpioPin) == LOW);
      if (isPressed && !macro.wasPressed) {
        logToWeb("Hardware-Taste registriert! Pin: " + String(macro.gpioPin) + ", Makro: " + macro.name);
        executeMacro(macro); 
        macro.wasPressed = true; 
        ws.textAll("ACK:" + macro.id); 
      } else if (!isPressed && macro.wasPressed) { 
        macro.wasPressed = false; 
      }
    }
  }
  delay(1); 
}
#include "webserver.h"
// Asynchroner HTTP-Webserver und WebSocket
#include <ESPAsyncWebServer.h>
// LittleFS-Dateisystem
#include <LittleFS.h>
// OTA-Firmware-Update
#include <Update.h>
#include <Preferences.h>
#include <WiFi.h>
// Eigene Module
#include "types.h"
#include "logger.h"
#include "pad.h"

// Globale Objekte (definiert in main.cpp)
extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern std::vector<MacroDefinition> myMacros;
extern int activeLayer;
extern Preferences preferences;
// Neustart-Flag (definiert in main.cpp)
extern bool shouldRestart;
extern unsigned long restartMillis;

// -----------------------------------------------------------------------
// WebSocket-Ereignis-Handler
// - Bei Verbindungsaufbau: aktiven Layer an den Client senden
// - TRIGGER:<id>  → Makro mit passender ID und aktivem Layer ausführen
// - SET_LAYER:<n> → Layer wechseln und alle Clients benachrichtigen
// -----------------------------------------------------------------------
static void onEvent(AsyncWebSocket* /*srv*/, AsyncWebSocketClient* client,
                    AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // Neuer Client: aktuellen Layer mitteilen
    client->text("LAYER_CHANGED:" + String(activeLayer));
  }
  else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    // Nur vollständige, ungefragmentierte Text-Frames verarbeiten
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0; // Null-Terminierung für String-Konvertierung
      String message = (char*)data;

      if (message.startsWith("TRIGGER:")) {
        // Makro anhand der ID im aktiven Layer suchen und ausführen
        String searchId = message.substring(8);
        for (const auto& macro : myMacros) {
          if (macro.id == searchId && macro.layer == activeLayer) {
            executeMacro(macro);
            ws.textAll("ACK:" + searchId); // Ausführung bestätigen
            return;
          }
        }
      }
      else if (message.startsWith("SET_LAYER:")) {
        // Layer wechseln und alle verbundenen Clients informieren
        activeLayer = message.substring(10).toInt();
        ws.textAll("LAYER_CHANGED:" + String(activeLayer));
        logToWeb("Layer gewechselt auf: " + String(activeLayer));
      }
    }
  }
}

// -----------------------------------------------------------------------
// Routen für den AP-Modus (Captive Portal)
// -----------------------------------------------------------------------
static void setupAPRoutes() {
  // Einfache Setup-Seite für WLAN-Konfiguration
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (LittleFS.exists("/html/ap_setup.html")) {
      request->send(LittleFS, "/html/ap_setup.html", "text/html");
    } else {
      request->send(404, "text/plain", "ap_setup.html fehlt!");
    }
  });

  // API: Liste aller verfügbaren WLANs scannen
  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* request) {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; ++i) {
      json += "{\"ssid\":\"" + String(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
      if (i < n - 1) json += ",";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  // API: WLAN-Daten speichern und verbinden
  server.on("/api/connect", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (request->hasParam("ssid") && request->hasParam("pass")) {
      String ssid = request->getParam("ssid")->value();
      String pass = request->getParam("pass")->value();

      preferences.begin("wifi-config", false);
      preferences.putString("ssid", ssid);
      preferences.putString("pass", pass);
      preferences.end();

      logToWeb("Versuche Verbindung zu: " + ssid);
      
      // Blocking call in Async handler is generally bad, but for setup it's often okay.
      // For a cleaner experience, we could use a separate task, but let's keep it simple.
      WiFi.begin(ssid.c_str(), pass.c_str());
      
      int c = 0;
      while (WiFi.status() != WL_CONNECTED && c < 20) {
        delay(500);
        c++;
      }

      if (WiFi.status() == WL_CONNECTED) {
        request->send(200, "text/plain", "Verbunden erfolgreich! Das Geraet startet neu...");
        shouldRestart = true;
        restartMillis = millis() + 1500;
      } else {
        request->send(500, "text/plain", "Verbindungsfehler. Bitte versuchen Sie es erneut.");
      }
    } else {
      request->send(400, "text/plain", "Fehlende Parameter.");
    }
  });

  // Alle übrigen GET-Anfragen: Datei direkt aus LittleFS ausliefern
  server.serveStatic("/", LittleFS, "/").setFilter([](AsyncWebServerRequest* request) {
    if (request->method() != HTTP_GET) return false; // Nur GET-Anfragen bedienen
    String path = request->url();
    if (path == "/") path = "/html/ap_setup.html";
    return LittleFS.exists(path);
  });

  // Alle anderen Anfragen auf die Startseite umleiten
  server.onNotFound([](AsyncWebServerRequest* request) {
    request->redirect("http://192.168.4.1/");
  });

}

// -----------------------------------------------------------------------
// Routen für den Station-Modus (normaler Betrieb)
// -----------------------------------------------------------------------
static void setupSTARoutes() {
  // WebSocket-Handler registrieren
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Helper zum Ausliefern von HTML-Dateien
  auto serveHtml = [](AsyncWebServerRequest* request, const char* path) {
    if (LittleFS.exists(path)) {
      request->send(LittleFS, path, "text/html");
    } else {
      request->send(404, "text/plain", String(path) + " fehlt!");
    }
  };

  // /debug – Live-Log-Anzeige im Browser
  server.on("/debug", HTTP_GET, [&](AsyncWebServerRequest* request) {
    serveHtml(request, "/html/debug.html");
  });

  // API: Liste aller Logs als JSON zurückgeben
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = "[";
    for (size_t i = 0; i < debugLog.size(); ++i) {
      json += "\"" + debugLog[i] + "\"";
      if (i < debugLog.size() - 1) json += ",";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  // Seiten ausliefern
  struct PageMapping { const char* url; const char* path; };
  PageMapping pages[] = {
    {"/", "/html/index.html"},
    {"/editor", "/html/editor.html"},
    {"/ide", "/html/ide.html"},
    {"/update", "/html/update.html"}
  };

  for (const auto& page : pages) {
    server.on(page.url, HTTP_GET, [&](AsyncWebServerRequest* request) { serveHtml(request, page.path); });
  }

  // API: Liste aller Dateien im LittleFS als JSON zurückgeben
  server.on("/api/listfiles", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = "{\"files\":[";
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
      if (json != "{\"files\":[") json += ",";
      json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
      file = root.openNextFile();
    }
    json += "]}";
    request->send(200, "application/json", json);
  });

  // API: Makro-Konfiguration speichern.
  // Unterstützt vier Übertragungsmethoden:
  //   1. JSON direkt im ersten Parameternamen (quirky IDE-Verhalten)
  //   2. Normales HTML-Formular (Schlüssel-Wert-Paare)
  //   3. Multipart-Upload
  //   4. Raw-Request-Body (mit optionalem X-Filename-Header)
  server.on("/api/macros/save", HTTP_POST,
    // --- Handler 1: Formular-Parameter ---
    [](AsyncWebServerRequest* request) {
      logToWeb("POST-Anfrage erhalten.");
      int params = request->params();
      if (params > 0) {
        logToWeb("Formular-Parameter gefunden: " + String(params));
        String targetFile = "/config/config.json";
        String fileContent = "";

        for (int i = 0; i < params; i++) {
          const AsyncWebParameter* param = request->getParam((size_t)i); // Expliziter Cast verhindert Überladungs-Ambiguität
          String pName = param->name();

          // Methode 1: IDE schickt rohes JSON fälschlicherweise im Parameternamen
          if (i == 0 && (pName.startsWith("{") || pName.startsWith("["))) {
            logToWeb("Erkenne JSON im ersten Parameter-Namen. Sichern...");
            File f = LittleFS.open("/config/config.json", "w");
            if (f) {
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
          if (pName == "path" || pName == "file" || pName == "filename") targetFile = param->value();
          if (pName == "code" || pName == "text" || pName == "content" || pName == "macros" || pName == "data") fileContent = param->value();
        }

        if (fileContent.length() > 0) {
          if (!targetFile.startsWith("/")) targetFile = "/" + targetFile;
          File f = LittleFS.open(targetFile, "w");
          if (f) {
            f.print(fileContent);
            f.close();
            logToWeb(targetFile + " erfolgreich via Form-Param ueberschrieben.");
            if (targetFile.endsWith("config.json")) loadConfig();
          }
          request->send(200, "text/plain", "OK");
          return;
        }
      }
      request->send(200, "text/plain", "Verarbeitet");
    },
    // --- Handler 2: Multipart-Upload ---
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
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
    // --- Handler 3: Raw-Body ---
    [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
      static File rawFile;
      if (index == 0) {
        String filename = "/config/config.json";
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

  // OTA-Update-Seite ausliefern
  server.on("/update", HTTP_GET, [&](AsyncWebServerRequest* request) { serveHtml(request, "/html/update.html"); });

  // OTA-Firmware-Upload über POST /update  (flasht den Firmware-Bereich, U_FLASH)
  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      bool ok = !Update.hasError();
      logToWeb(ok ? "OTA-Firmware erfolgreich. Neustart in 1s..." : "OTA-Firmware fehlgeschlagen: " + String(Update.errorString()));
      request->send(200, "text/plain", ok ? "Firmware-Update erfolgreich! Das Geraet startet neu..." : "Fehler: " + String(Update.errorString()));
      if (ok) { shouldRestart = true; restartMillis = millis() + 1500; }
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (index == 0) {
        logToWeb("OTA-Firmware-Upload gestartet: " + filename);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
          logToWeb("Update.begin(U_FLASH) fehlgeschlagen: " + String(Update.errorString()));
        }
      }
      if (Update.isRunning()) {
        if (Update.write(data, len) != len) {
          logToWeb("Update.write() Fehler: " + String(Update.errorString()));
        }
      }
      if (final) {
        if (Update.end(true)) { logToWeb("Firmware-Upload fertig: " + String(index + len) + " Bytes."); }
        else                  { logToWeb("Update.end() fehlgeschlagen: " + String(Update.errorString())); }
      }
    }
  );

  // OTA-Dateisystem-Upload über POST /update-fs  (flasht den LittleFS-Bereich, U_SPIFFS)
  // Datei: .pio/build/<env>/littlefs.bin  (PlatformIO-Target: buildfs)
  server.on("/update-fs", HTTP_POST,
    [](AsyncWebServerRequest* request) {
      bool ok = !Update.hasError();
      logToWeb(ok ? "OTA-Dateisystem erfolgreich. Neustart in 1s..." : "OTA-Dateisystem fehlgeschlagen: " + String(Update.errorString()));
      request->send(200, "text/plain", ok ? "Dateisystem-Update erfolgreich! Das Geraet startet neu..." : "Fehler: " + String(Update.errorString()));
      if (ok) { shouldRestart = true; restartMillis = millis() + 1500; }
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (index == 0) {
        logToWeb("OTA-Dateisystem-Upload gestartet: " + filename);
        // U_SPIFFS flasht die LittleFS-Partition (gleiche Partition, anderer Name)
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
          logToWeb("Update.begin(U_SPIFFS) fehlgeschlagen: " + String(Update.errorString()));
        }
      }
      if (Update.isRunning()) {
        if (Update.write(data, len) != len) {
          logToWeb("Update.write() Fehler: " + String(Update.errorString()));
        }
      }
      if (final) {
        if (Update.end(true)) { logToWeb("Dateisystem-Upload fertig: " + String(index + len) + " Bytes."); }
        else                  { logToWeb("Update.end() fehlgeschlagen: " + String(Update.errorString())); }
      }
    }
  );

  // API: Firmware-Version zurückgeben
  server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"version\":\"" FIRMWARE_VERSION "\"}");
  });

  // API: Aktuelle config.json als JSON ausliefern
  server.on("/api/macros", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (LittleFS.exists("/config/config.json")) {
      request->send(LittleFS, "/config/config.json", "application/json");
    } else {
      request->send(404, "text/plain", "Config fehlt!");
    }
  });

  // Alle übrigen GET-Anfragen: Datei direkt aus LittleFS ausliefern
  server.serveStatic("/", LittleFS, "/").setFilter([](AsyncWebServerRequest* request) {
    if (request->method() != HTTP_GET) return false; // Nur GET-Anfragen bedienen
    String path = request->url();
    if (path == "/") path = "/html/index.html";
    return LittleFS.exists(path);
  });

  server.onNotFound([](AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "404: Nicht gefunden!");
  });
}

// -----------------------------------------------------------------------
// Öffentliche Funktion: Webserver-Routen einrichten und Server starten
// -----------------------------------------------------------------------
void setupWebserver(bool isAP) {
  if (isAP) {
    setupAPRoutes();
  } else {
    setupSTARoutes();
  }
  server.begin(); // HTTP-Server starten
}

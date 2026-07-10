// Arduino-Framework-Grundlage
#include <Arduino.h>
// WLAN-Funktionen des ESP32
#include <WiFi.h>
// Asynchroner HTTP-Webserver und WebSocket
#include <ESPAsyncWebServer.h>
// DNS-Server für Captive Portal im AP-Modus
#include <DNSServer.h>
// Persistente Schlüssel-Wert-Speicherung im Flash (NVS)
#include <Preferences.h>
// LittleFS-Dateisystem im Flash
#include <LittleFS.h>
// USB-Stack des ESP32-S3
#include "USB.h"
// USB-HID-Tastatur-Emulation
#include "USBHIDKeyboard.h"
// USB-HID-Consumer-Control (Medientasten)
#include "USBHIDConsumerControl.h"

// Eigene Module
#include "types.h"
#include "logger.h"
#include "pad.h"
#include "webserver.h"

// -----------------------------------------------------------------------
// Globale Objekte – hier definiert, in anderen Modulen per extern genutzt
// -----------------------------------------------------------------------

// HTTP-Webserver auf Port 80
AsyncWebServer server(80);
// WebSocket-Endpunkt unter /ws
AsyncWebSocket ws("/ws");
// DNS-Server für Captive Portal
DNSServer dnsServer;
// NVS-Speicher für WLAN-Zugangsdaten
Preferences preferences;
// USB-HID-Tastatur
USBHIDKeyboard Keyboard;
// USB-HID-Mediensteuerung
USBHIDConsumerControl ConsumerControl;

// Liste aller geladenen Makros
std::vector<MacroDefinition> myMacros;
// Aktuell aktiver Layer (0 = Standardlayer)
int activeLayer = 0;

// true, wenn der ESP32 im Access-Point-Modus läuft
bool isAPMode = false;
// Gespeicherte WLAN-Zugangsdaten
String ssidStore, passStore;

// Flag und Zeitpunkt für einen geplanten Neustart
bool shouldRestart = false;
unsigned long restartMillis = 0;

// -----------------------------------------------------------------------
// Initialisierung: wird einmalig beim Start ausgeführt
// -----------------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // LittleFS mounten (true = automatisch formatieren, falls nötig)
  if (!LittleFS.begin(true)) { Serial.println("LittleFS Mount Failed!"); }

  // Makros aus config.json laden
  loadConfig();

  // GPIO-Pins für Hardware-Tasten konfigurieren (interner Pull-Up)
  for (const auto& macro : myMacros) {
    if (macro.gpioPin != NO_PIN && macro.gpioPin > 0) {
      pinMode(macro.gpioPin, INPUT_PULLUP);
    }
  }

  // USB-HID-Geräte initialisieren
  Keyboard.begin(); ConsumerControl.begin(); USB.begin();

  // Gespeicherte WLAN-Zugangsdaten aus dem NVS lesen
  preferences.begin("wifi-config", false);
  ssidStore = preferences.getString("ssid", "");
  passStore = preferences.getString("pass", "");

  // Verbindung zum gespeicherten WLAN herstellen (max. 15 × 500 ms = 7,5 s)
  WiFi.mode(WIFI_STA);
  if (ssidStore != "") {
    WiFi.begin(ssidStore.c_str(), passStore.c_str());
    int c = 0;
    while (WiFi.status() != WL_CONNECTED && c < 15) { delay(500); c++; }
  }

  if (WiFi.status() != WL_CONNECTED) {
    // Kein WLAN verfügbar → Access-Point-Modus mit Captive Portal starten
    isAPMode = true;
    WiFi.disconnect(); WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
    WiFi.softAP("0x90-deck-Setup");
    // DNS-Server leitet alle Anfragen auf das Captive Portal um
    delay(500);
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
  }

  // Webserver-Routen einrichten und Server starten
  setupWebserver(isAPMode);
}

// -----------------------------------------------------------------------
// Hauptschleife: wiederholt ausgeführt
// -----------------------------------------------------------------------
void loop() {
  // Geplanten Neustart ausführen, sobald der Zeitstempel erreicht ist
  if (shouldRestart && millis() > restartMillis) { ESP.restart(); }

  // Im AP-Modus: DNS-Anfragen weiterleiten und sofort zurückkehren
  if (isAPMode) { dnsServer.processNextRequest(); delay(10); return; }

  // Inaktive WebSocket-Clients bereinigen
  ws.cleanupClients();

  // Hardware-Tasten pollen: nur Makros des aktiven Layers mit zugewiesenem Pin prüfen
  for (auto& macro : myMacros) {
    if (macro.gpioPin != NO_PIN && macro.gpioPin > 0 && macro.layer == activeLayer) {
      // LOW = gedrückt (INPUT_PULLUP: aktiv-niedrig)
      bool isPressed = (digitalRead(macro.gpioPin) == LOW);
      if (isPressed && !macro.wasPressed) {
        // Flanke: Taste wurde gerade gedrückt → Makro auslösen
        logToWeb("Hardware-Taste registriert! Pin: " + String(macro.gpioPin) + ", Makro: " + macro.name);
        executeMacro(macro);
        macro.wasPressed = true;
        ws.textAll("ACK:" + macro.id); // Web-UI über Ausführung informieren
      } else if (!isPressed && macro.wasPressed) {
        // Flanke: Taste wurde losgelassen → Zustand zurücksetzen
        macro.wasPressed = false;
      }
    }
  }
  delay(1); // Kurze Pause zur CPU-Entlastung
}
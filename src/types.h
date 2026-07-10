#pragma once
// Arduino-Basis für String, uint16_t usw.
#include <Arduino.h>
// Dynamische Arrays
#include <vector>

// Platzhalter: kein GPIO-Pin zugewiesen
#define NO_PIN -1
// Aktuelle Konfigurationsversion
const int CURRENT_VERSION = 1;

// Aktionstypen für Makros:
// TYPE_HOTKEY           = Tastenkombination (z. B. CTRL+C)
// TYPE_CONSUMER_CONTROL = Medientaste (z. B. Lautstärke)
// TYPE_TEXT_SEQUENCE    = Zeichenfolge, Taste für Taste gesendet
enum ActionType {
  TYPE_HOTKEY           = 0,
  TYPE_CONSUMER_CONTROL = 1,
  TYPE_TEXT_SEQUENCE    = 2
};

// Beschreibt ein einzelnes Makro mit allen zugehörigen Parametern
struct MacroDefinition {
  String id;                     // Eindeutige ID des Makros
  String name;                   // Anzeigename
  ActionType type;               // Art der Aktion
  String rawKeysString;          // Rohe Tastenfolge als String (z. B. "CTRL+C")
  std::vector<uint16_t> keys;    // Geparste HID-Keycodes
  int gpioPin;                   // Zugewiesener GPIO-Pin (-1 = kein Pin)
  bool wasPressed;               // Entprellungs-Zustand: war Taste zuvor gedrückt?
  unsigned long lastRepeat;      // Zeitstempel für Wiederholungslogik
  String icon;                   // Icon-Bezeichner für das Web-UI
  int layer;                     // Layer, dem dieses Makro zugeordnet ist
};

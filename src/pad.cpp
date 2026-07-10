#include "pad.h"
#include "logger.h"
// JSON-Parser
#include <ArduinoJson.h>
// LittleFS-Dateisystem
#include <LittleFS.h>
// USB-HID-Tastatur
#include "USBHIDKeyboard.h"
// USB-HID-Mediensteuerung
#include "USBHIDConsumerControl.h"

// Globale Makro-Liste und aktiver Layer (definiert in main.cpp)
extern std::vector<MacroDefinition> myMacros;
extern int activeLayer;
// USB-HID-Geräte (definiert in main.cpp)
extern USBHIDKeyboard Keyboard;
extern USBHIDConsumerControl ConsumerControl;

// Wandelt einen Tastenkürzel-String (z. B. "CTRL+SHIFT+T") in HID-Keycodes um.
// Bei Consumer-Control-Aktionen werden die USB-HID-Konsumentencodes genutzt.
std::vector<uint16_t> parseKeys(String keyStr, ActionType type) {
  std::vector<uint16_t> keys;
  if (type == TYPE_CONSUMER_CONTROL) {
    // HID-Consumer-Control-Keycodes
    if      (keyStr == "MUTE")     keys.push_back(0xE2);
    else if (keyStr == "VOL_UP")   keys.push_back(0xE9);
    else if (keyStr == "VOL_DOWN") keys.push_back(0xEA);
  } else {
    // Tastenkombination durch '+' getrennt tokenisieren
    int startIndex = 0;
    int endIndex   = keyStr.indexOf('+');

    while (startIndex < (int)keyStr.length()) {
      String token;
      if (endIndex == -1) {
        token      = keyStr.substring(startIndex);
        startIndex = keyStr.length();
      } else {
        token      = keyStr.substring(startIndex, endIndex);
        startIndex = endIndex + 1;
        endIndex   = keyStr.indexOf('+', startIndex);
      }

      token.trim();
      if (token.length() == 0) continue;

      // Sonderzeichen-Tokens auf HID-Keycodes mappen
      if      (token == "CTRL")      keys.push_back(KEY_LEFT_CTRL);
      else if (token == "SHIFT")     keys.push_back(KEY_LEFT_SHIFT);
      else if (token == "ALT")       keys.push_back(KEY_LEFT_ALT);
      else if (token == "ENTER")     keys.push_back(KEY_RETURN);
      else if (token == "SPACE")     keys.push_back(' ');
      else if (token == "BACKSPACE") keys.push_back(KEY_BACKSPACE);
      else if (token == "ESC")       keys.push_back(KEY_ESC);
      else if (token == "DELETE")    keys.push_back(KEY_DELETE);
      else if (token == "INSERT")    keys.push_back(KEY_INSERT);
      else if (token == "HOME")      keys.push_back(KEY_HOME);
      else if (token == "END")       keys.push_back(KEY_END);
      else if (token == "PAGE_UP")   keys.push_back(KEY_PAGE_UP);
      else if (token == "PAGE_DOWN") keys.push_back(KEY_PAGE_DOWN);
      else if (token == "UP")        keys.push_back(KEY_UP_ARROW);
      else if (token == "DOWN")      keys.push_back(KEY_DOWN_ARROW);
      else if (token == "LEFT")      keys.push_back(KEY_LEFT_ARROW);
      else if (token == "RIGHT")     keys.push_back(KEY_RIGHT_ARROW);
      else if (token == "F1")        keys.push_back(KEY_F1);
      else if (token == "F2")        keys.push_back(KEY_F2);
      else if (token == "F3")        keys.push_back(KEY_F3);
      else if (token == "F4")        keys.push_back(KEY_F4);
      else if (token == "F5")        keys.push_back(KEY_F5);
      else if (token == "F6")        keys.push_back(KEY_F6);
      else if (token == "F7")        keys.push_back(KEY_F7);
      else if (token == "F8")        keys.push_back(KEY_F8);
      else if (token == "F9")        keys.push_back(KEY_F9);
      else if (token == "F10")       keys.push_back(KEY_F10);
      else if (token == "F11")       keys.push_back(KEY_F11);
      else if (token == "F12")       keys.push_back(KEY_F12);
      else if (token.length() == 1) {
        // Einzelnes druckbares Zeichen direkt als ASCII-Keycode übernehmen
        keys.push_back((uint16_t)token.charAt(0));
      }
    }
  }
  return keys;
}

// Liest die Makro-Konfiguration aus /config.json im LittleFS
// und befüllt den globalen myMacros-Vector.
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

  myMacros.clear(); // Vorherige Makros verwerfen
  JsonArray arr = doc["macros"].as<JsonArray>();
  for (JsonObject obj : arr) {
    MacroDefinition m;
    m.id          = obj["id"].as<String>();
    m.name        = obj["name"].as<String>();
    m.type        = (ActionType)obj["type"].as<int>();
    m.gpioPin     = obj["gpio"].as<int>();
    m.icon        = obj["icon"].as<String>();
    m.layer       = obj["layer"].as<int>();
    m.wasPressed  = false;
    m.lastRepeat  = 0;

    // Rohe Tastenfolge parsen und als HID-Keycodes speichern
    String rawKeys   = obj["keys"].as<String>();
    m.rawKeysString  = rawKeys;
    m.keys           = parseKeys(rawKeys, m.type);
    myMacros.push_back(m);
  }
  logToWeb("Konfiguration erfolgreich geladen. Makros geladen: " + String(myMacros.size()));
}

// Führt ein Makro über USB-HID aus.
// Je nach Typ: Medientaste, Zeichenfolge oder Tastenkombination.
void executeMacro(const MacroDefinition& macro) {
  logToWeb("Fuehre Makro aus: '" + macro.name + "' (Typ: " + String(macro.type) + ", Keys: " + macro.rawKeysString + ")");

  if (macro.type == TYPE_CONSUMER_CONTROL) {
    // Medientaste drücken und nach kurzem Delay wieder loslassen
    for (uint16_t key : macro.keys) { ConsumerControl.press(key); }
    delay(10);
    ConsumerControl.release();
  }
  else if (macro.type == TYPE_TEXT_SEQUENCE) {
    // Jede Taste einzeln mit Pause drücken und loslassen (Zeichen-für-Zeichen)
    for (uint16_t key : macro.keys) {
      Keyboard.press(key);
      delay(15);
      Keyboard.release(key);
      delay(10);
    }
  }
  else {
    // Alle Tasten gleichzeitig drücken (Hotkey-Kombination), dann loslassen
    for (uint16_t key : macro.keys) { Keyboard.press(key); }
    delay(30);
    Keyboard.releaseAll();
  }
}

#pragma once
#include "types.h"
#include <vector>

// Wandelt einen Tastenk\u00fcrzel-String (z. B. "CTRL+SHIFT+T") in HID-Keycodes um.
std::vector<uint16_t> parseKeys(String keyStr, ActionType type);

// Liest die Makro-Konfiguration aus /config.json im LittleFS
// und bef\u00fcllt den globalen myMacros-Vector.
void loadConfig();

// F\u00fchrt ein Makro \u00fcber USB-HID aus.
void executeMacro(const MacroDefinition& macro);

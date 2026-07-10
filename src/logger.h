#pragma once
#include <Arduino.h>
#include <vector>

// Ringpuffer für Log-Nachrichten (abrufbar unter /debug im Browser)
extern std::vector<String> debugLog;
// Maximale Anzahl gespeicherter Log-Zeilen
extern const size_t MAX_LOG_LINES;

// Schreibt eine Nachricht in den seriellen Monitor und den Web-Log-Puffer.
// Älteste Einträge werden bei Überschreiten von MAX_LOG_LINES verworfen.
void logToWeb(String message);

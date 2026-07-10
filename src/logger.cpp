#include "logger.h"

// Ringpuffer-Definition
std::vector<String> debugLog;
const size_t MAX_LOG_LINES = 50;

void logToWeb(String message) {
  unsigned long timestamp = millis();
  String line = "[" + String(timestamp) + "] " + message;
  Serial.println(line);
  debugLog.push_back(line);
  if (debugLog.size() > MAX_LOG_LINES) {
    debugLog.erase(debugLog.begin()); // Ältesten Eintrag entfernen
  }
}

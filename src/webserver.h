#pragma once

// Richtet alle HTTP-Routen und den WebSocket-Handler ein.
// isAP = true  → nur Captive-Portal-Routen (AP-Modus)
// isAP = false → alle API- und Datei-Routen (Station-Modus)
void setupWebserver(bool isAP);

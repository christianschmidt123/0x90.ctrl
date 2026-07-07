# 0x90.deck 🎛️

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-blue.svg)](https://www.espressif.com/)

**0x90.deck** ist ein komplett offenes, plattformunabhängiges DIY-Streamdeck und Makro-Pad auf Basis des ESP32-S3. Der Name ist Programm: `0x90` steht im MIDI-Protokoll für den Befehl *Note On* – das perfekte Präfix für ein Tool, das deine Workflows per Tastendruck zum Leben erweckt.

Das Besondere: Es simuliert eine echte USB-Tastatur (HID) direkt über die native USB-Schnittstelle des ESP32. Die gesamte Konfiguration läuft kabellos über ein integriertes, modernes Web-Interface, das direkt vom Mikrocontroller gehostet wird. Kein Treiber-Installations-Zwang, keine Cloud!

---

## ✨ Features

* **Native USB-HID Emulation:** Funktioniert ohne Software auf Windows, macOS und Linux. Wird vom PC als echte Tastatur/Mediensteuerung erkannt.
* **Visueller Hotkey-Editor:** Baue komplexe Tastatur-Kombinationen (z. B. `CTRL+SHIFT+ALT+DELETE` oder `F1`–`F12`) direkt im Browser zusammen.
* **Integrierter Icon-Picker:** Wähle Symbole für deine Makros visuell aus dem integrierten Bootstrap-Iconset.
* **Web-IDE & Dateibrowser:** Bearbeite die Konfigurationsdateien oder das HTML-Interface dank des integrierten *Ace-Editors* direkt live auf dem Dateisystem (`LittleFS`) des Decks.
* **Multi-Layer Support:** Wechsle flexibel zwischen verschiedenen Tastenbelegungen (z. B. Layer 0 für Gaming, Layer 1 für Videoschnitt).
* **Kabellose OTA-Updates:** Flashe neue Firmware-Versionen (`.bin`) ganz einfach over-the-air über die Weboberfläche.

---

## 🛠️ Hardware-Anforderungen

Das Projekt ist für den **ESP32-S3-DevKitC-1** (oder baugleiche Boards mit nativem USB) optimiert.

* **Controller:** ESP32-S3 (Wichtig: Der USB-Port muss direkt mit den GPIOs 19 und 20 verbunden sein für USB-HID).
* **Tasten/Schalter:** Beliebige mechanische Taster (z. B. Cherry MX) direkt an die GPIOs angeschlossen (GND-Schaltung via `INPUT_PULLUP`).
* **Gehäuse:** Bereit für den 3D-Drucker (STL-Dateien folgen im Ordner `/hardware`).

---

## 🚀 Software-Setup & Installation

Das Projekt basiert auf **PlatformIO** (VS Code).

### 1. Repository klonen
```bash
git clone [https://github.com/DEIN_GITHUB_NAME/0x90.deck.git](https://github.com/DEIN_GITHUB_NAME/0x90.deck.git)
cd 0x90.deck
```
### 2. Dateisystem (LittleFS) vorbereiten
Die Benutzeroberfläche ist vom C++ Code getrennt und liegt im Ordner data/. Dieser muss vor dem ersten Start auf den ESP32 geladen werden:

Öffne das Projekt in PlatformIO.

Navigiere zum PlatformIO-Menü (Ameisen-Icon).

Wähle dein Board aus und klicke unter Platform auf Upload Filesystem Image.

### 3. Firmware flashen
Klicke auf Upload and Monitor, um den C++ Code zu kompilieren und auf das Board zu spielen.

🌐 Erste Schritte & Nutzung
Nach dem ersten Start spannt das 0x90.deck ein eigenes WLAN auf: ESP32-MacroPad-Setup.

Verbinde dich mit dem WLAN und öffne im Browser die IP 192.168.4.1, um deine Heim-WLAN-Daten zu hinterlegen.

Sobald das Deck in deinem Netzwerk ist, erreichst du die Interfaces über folgende Pfade:

http://<esp-ip>/ – Die Hauptseite (Dashboard).

http://<esp-ip>/editor – Der visuelle Hotkey- & Icon-Konfigurator.

http://<esp-ip>/ide – Die integrierte Web-IDE für Live-Code-Anpassungen.

http://<esp-ip>/update – Das kabellose OTA-Firmware-Update.

📂 Projektstruktur
```Plaintext
├── data/               # Web-Dateien für das LittleFS Dateisystem
│   ├── index.html      # Hauptseite / Dashboard
│   ├── editor.html     # Visueller Hotkey- und Icon-Picker
│   ├── ide.html        # Web-IDE mit Ace-Editor & Dateibrowser
│   └── update.html     # OTA-Update Maske
├── src/
│   └── main.cpp        # C++ Kern-Logik (Webserver, WebSocket, HID-Treiber)
├── platformio.ini      # PlatformIO Konfiguration & Abhängigkeiten
└── README.md
```
🤝 Mitwirken (Contributing)
Das Projekt ist zu 100% Open Source. Wenn du Fehler findest, neue Tastencodes implementieren möchtest oder das Web-Interface verbessern willst:

Forke das Projekt.

Erstelle einen Feature-Branch (git checkout -b feature/AmazingFeature).

Commitest deine Änderungen (git commit -m 'Add some AmazingFeature').

Pushe den Branch (git push origin feature/AmazingFeature).

Öffne einen Pull Request.

📄 Lizenz
Dieses Projekt ist unter der MIT-Lizenz lizenziert – siehe die LICENSE Datei für Details.

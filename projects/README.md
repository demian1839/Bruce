# ESP32-P4 Host + Onboard ESP32-C6 + Elecrow Display Terminal (ESP-NOW) Firmware Project

Dieses Projekt bietet eine vollständige ESP-IDF Firmware-Lösung zur Kopplung eines **ESP32-P4 Boards mit Onboard-ESP32-C6 Wireless Co-Prozessor** und eines **Elecrow Display-Panels (ESP32/ESP32-S3)** über **ESP-NOW** (Latenz < 10 ms).

---

## 1. System-Architektur & Rollenverteilung

```
+------------------------------------+           ESP-NOW            +------------------------------------+
|        Elecrow Display             |  <------------------------>  |          Onboard ESP32-C6          |
|   (Anzeige- & Eingabeterminal)     |   Touch-Pakete (Opcode 0x01) |      (Wireless Co-Prozessor)       |
|                                    |   ------------------------>  |                 |                  |
| - Touch-Eingaben erfassen          |                              |             SPI / Link             |
| - Rendern von Zustandswerten/UI    |   UI-Updates   (Opcode 0x02) |                 v                  |
| - Direct Peer-to-Peer              |  <-------------------------  |         ESP32-P4 Host MCU          |
|                                    |                              |   (Systemlogik & Berechnungen)     |
+------------------------------------+                              +------------------------------------+
```

### Rollenverteilung
* **ESP32-P4 System unit (`p4_c6_host`):** Dient als Hauptrecheneinheit für alle komplexen Berechnungen und Systemlogiken.
* **Onboard ESP32-C6 Co-Prozessor (`c6_espnow_coprocessor`):** Steuert das Wi-Fi/ESP-NOW Funkmodul im 2.4 GHz Band auf Channel 1 an und stellt die drahtlose Brücke zum Elecrow Display dar.
* **Elecrow Display Terminal (`elecrow_display`):** Fungiert als reines Eingabe- und Anzeigeterminal (Touch-Events senden, Visualisierungsdaten empfangen).
* **Drahtlose Kommunikation:** Direkt über **ESP-NOW** zwischen dem Onboard-ESP32-C6 und dem Elecrow-Panel ohne externen WLAN-Router.

---

## 2. Paketstruktur (`projects/common/protocol.h`)

Die Datenübertragung erfolgt in schlanken Byte-Paketen über ESP-NOW:

* **Header:**
  * `magic`: `0xA5` (Protocol Magic)
  * `opcode`: `0x01` (`OP_TOUCH_EVENT`), `0x02` (`OP_UI_UPDATE`), `0x03` (`OP_HEARTBEAT`)
  * `payload_len`: Länge der Nutzdaten
* **Touch Event Payload (`OP_TOUCH_EVENT`):**
  * `timestamp_ms`: Zeitstempel in ms
  * `touch_count`: Anzahl der Touch-Punkte
  * `points`: Array aus X/Y-Koordinaten (uint16_t) und Status (Released/Pressed/Moved)
* **UI Update Payload (`OP_UI_UPDATE`):**
  * `sequence`: Paketsequenznummer
  * `uptime_ms`: Uptime in ms
  * `calc_counter`: Vom P4 berechneter Zähler/Zustandswert
  * `active_widget_id`: ID des aktiven UI-Elements
  * `bg_red`, `bg_green`, `bg_blue`: Farbwerte für UI/Hintergrund
  * `status_text`: Textnachricht (max. 32 Zeichen)
* **Checksum & Optimierte Paketlänge:**
  * XOR-Prüfsumme über den gesamten Header und Payload.
  * Dynamische Paketgrößenberechnung über `get_packet_total_size()` zur Vermeidung unnötigen Datentransfers.

---

## 3. Ordnerstruktur

```
projects/
├── common/
│   └── protocol.h                 # Gemeinsame Protokoll- & Paketdefinitionen
├── p4_c6_host/                    # ESP-IDF Projekt für ESP32-P4 Hauptprozessor
│   ├── CMakeLists.txt
│   ├── sdkconfig
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c
├── c6_espnow_coprocessor/         # ESP-IDF Projekt für Onboard ESP32-C6 Funk-Coprozessor
│   ├── CMakeLists.txt
│   ├── sdkconfig
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c
├── elecrow_display/               # ESP-IDF Projekt für Elecrow Display Panel
│   ├── CMakeLists.txt
│   ├── sdkconfig
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c
└── README.md                      # Diese Anleitung
```

---

## 4. Build & Flash Anweisungen

### Voraussetzungen
1. **ESP-IDF v5.1+** installiert und Umgebung geladen:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

---

### A) ESP32-P4 System Host

1. **In das P4-Projekt wechseln:**
   ```bash
   cd projects/p4_c6_host
   ```

2. **Target festlegen & Firmware bauen:**
   ```bash
   idf.py set-target esp32p4
   idf.py build
   ```
   *Erzeugt die Binärdatei `build/p4_c6_host.bin`.*

3. **Flashen:**
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   *Oder via `esptool.py`:*
   ```bash
   esptool.py --chip esp32p4 -p /dev/ttyUSB0 -b 460800 write_flash 0x10000 build/p4_c6_host.bin
   ```

---

### B) Onboard ESP32-C6 Wireless Co-Prozessor

1. **In das C6 Coprozessor-Projekt wechseln:**
   ```bash
   cd projects/c6_espnow_coprocessor
   ```

2. **Target festlegen & Firmware bauen:**
   ```bash
   idf.py set-target esp32c6
   idf.py build
   ```
   *Erzeugt die Binärdatei `build/c6_espnow_coprocessor.bin`.*

3. **Flashen:**
   ```bash
   idf.py -p /dev/ttyUSB1 flash monitor
   ```
   *Oder via `esptool.py`:*
   ```bash
   esptool.py --chip esp32c6 -p /dev/ttyUSB1 -b 460800 write_flash 0x10000 build/c6_espnow_coprocessor.bin
   ```

---

### C) Elecrow Display Panel Terminal

1. **In das Display-Projekt wechseln:**
   ```bash
   cd projects/elecrow_display
   ```

2. **Target festlegen (z.B. ESP32-S3) & Firmware bauen:**
   ```bash
   idf.py set-target esp32s3
   idf.py build
   ```
   *Erzeugt die Binärdatei `build/elecrow_display.bin`.*

3. **Flashen:**
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```
   *Oder via `esptool.py`:*
   ```bash
   esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 460800 write_flash 0x10000 build/elecrow_display.bin
   ```

---

## 5. Konfiguration der Peer-Registrierung (MAC-Adressen)

Das System ist standardmäßig für **Broadcast-Pairing (`FF:FF:FF:FF:FF:FF`)** konfiguriert, sodass sich die Geräte beim ersten Hochfahren automatisch finden.

Für eine gezielte Peer-to-Peer Verbindung:
1. Beim Starten aller Boards werden die Station-MAC-Adressen in den Log-Meldungen ausgegeben.
2. Trage in `projects/elecrow_display/main/main.c` die Ziel-MAC des C6 Coprozessors ein:
   ```c
   static uint8_t s_p4_host_mac[ESP_NOW_ETH_ALEN] = { 0x24, 0xDC, 0xC3, 0xAA, 0xBB, 0xCC };
   ```
3. Trage in `projects/c6_espnow_coprocessor/main/main.c` die Ziel-MAC des Elecrow Panels ein:
   ```c
   static uint8_t s_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0x30, 0x30, 0xF9, 0x11, 0x22, 0x33 };
   ```

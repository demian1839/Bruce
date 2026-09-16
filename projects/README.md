# ESP32-P4 Host + Onboard ESP32-C6 + Elecrow Display Terminal (ESP-NOW) Firmware Project

Dieses Projekt bietet eine vollständige ESP-IDF Firmware-Lösung zur Kopplung eines **ESP32-P4 Boards mit Onboard-ESP32-C6 Wireless Co-Prozessor** und eines **Elecrow Display-Panels (ESP32/ESP32-S3 mit 4 MB Flash)** über **ESP-NOW** (Latenz < 10 ms).

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
  * `checksum`: XOR-Prüfsumme im Header
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
├── elecrow_display/               # ESP-IDF Projekt für Elecrow Display Panel (4MB Flash)
│   ├── CMakeLists.txt
│   ├── partitions_4mb.csv         # Custom Partition Table für große .bin Dateien (bis 3.6 MB)
│   ├── sdkconfig
│   └── main/
│       ├── CMakeLists.txt
│       └── main.c
└── README.md                      # Diese Anleitung
```

---

## 4. Build & Flash Anweisungen

### A) ESP32-P4 System Host

1. **In das P4-Projekt wechseln:**
   ```bash
   cd projects/p4_c6_host
   idf.py set-target esp32p4
   idf.py build
   ```
2. **Flashen auf ESP32-P4:**
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

---

### B) Onboard ESP32-C6 Wireless Co-Prozessor

1. **In das C6 Coprozessor-Projekt wechseln:**
   ```bash
   cd projects/c6_espnow_coprocessor
   idf.py set-target esp32c6
   idf.py build
   ```
2. **Flashen auf Onboard ESP32-C6:**
   ```bash
   idf.py -p /dev/ttyUSB1 flash monitor
   ```

---

### C) Elecrow Display Panel Terminal (4 MB Flash / 4000 KB)

Das Elecrow Projekt nutzt ein angepasstes Partitionsschema (`partitions_4mb.csv`), um auch größere Firmwares (z. B. **2.1 MB** bis **3.6 MB**) problemlos unterzubringen.

1. **In das Display-Projekt wechseln & bauen:**
   ```bash
   cd projects/elecrow_display
   idf.py set-target esp32s3   # bzw. esp32 je nach Elecrow Panel
   idf.py build
   ```

2. **Flashen auf Elecrow Panel:**
   ```bash
   idf.py -p /dev/ttyACM0 flash monitor
   ```
   *Oder manuell mit `esptool.py`:*
   ```bash
   esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 460800 write_flash 0x10000 build/elecrow_display.bin
   ```

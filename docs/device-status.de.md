# Gerätestatus im lokalen WLAN

Unter **Others → Geraetestatus** gibt es eine getrennte Statuskopplung. Empfangene Nachrichten starten **keine** Bruce-Funktionen, Skripte, Funkaktionen oder Befehle. Es gibt keine Verbindung zu Dateiübertragung oder Command-Dispatchern.

## Auffindbarkeit

- Der Hintergrunddienst startet automatisch mit der Firmware, unabhängig vom geöffneten Menü.
- Sobald eine WLAN-Verbindung oder ein WLAN-Access-Point mit IP-Adresse besteht, sendet er alle drei Sekunden UDP-Beacons auf Port **47653**.
- Er schaltet WLAN nicht selbst ein und verändert weder Kanal noch Zugangsdaten.
- **„In der Nähe“ bedeutet dasselbe lokale WLAN/Subnetz**, nicht physische Nähe. Geräte in anderen Netzen werden nicht gefunden. Client-Isolation, Broadcast-Filter und Firewalls können die Erkennung verhindern.
- Ohne WLAN, im Schlafmodus oder bei unterbrochener Funkverbindung ist das Gerät nicht auffindbar. Ein Funkkanalwechsel durch andere Funktionen kann die Verbindung unterbrechen.
- Die Listen werden beim Öffnen und über **Aktualisieren** neu eingelesen. Der Dienst läuft auch bei geschlossenem Menü weiter.

## Koppeln

1. Beide Geräte mit demselben vertrauenswürdigen WLAN verbinden; alternativ eines als Hotspot betreiben und das andere damit verbinden.
2. Auf Gerät A **Kopplung anbieten** öffnen. Das Angebot gilt 60 Sekunden; seine zufällige Geräte-ID und ein 32-stelliger Hex-Schlüssel erscheinen auf dem Display.
3. Auf Gerät B **Geraete in der Naehe** öffnen, gegebenenfalls aktualisieren und die auf A angezeigte ID wählen.
4. Den Schlüssel eingeben und **Nur Status koppeln** bestätigen.
5. Auf A die ID des Anfragenden prüfen und die Statuskopplung ausdrücklich bestätigen.
6. Unter **Gekoppelte Geraete** nachsehen. Unter **Status senden** lassen sich ausschließlich **Bereit**, **Beschaeftigt**, **Akku niedrig** und **Hallo** senden. Diese Werte werden manuell gewählt; es gibt keine automatische Weiterleitung von Geräteaktionen und keine Weiterverteilung empfangener Meldungen.

Maximal vier Gegenstellen und acht gefundene Geräte werden verwaltet. Statusmeldungen werden per UDP ohne Zustellgarantie übertragen. Heartbeats erfolgen alle drei Sekunden, inaktive Kopplungen verschwinden nach etwa 15 Sekunden. Unbestätigte Anfragen laufen nach 60 Sekunden ab. **Alle trennen** beendet lokale Kopplungen sofort; geht die Trennmeldung verloren, erkennt die Gegenstelle das über den Timeout.

## Sicherheits- und Datenschutzgrenzen

- Discovery-Beacons sind absichtlich unbeglaubigt und können gefälscht werden. Eine Anzeige in der Liste ist kein Identitätsnachweis.
- Kopplung erfordert den zufälligen 128-Bit-Schlüssel und Bestätigung auf beiden Geräten. Ein Angebot ist einmal verwendbar und wird danach verworfen.
- HMAC-SHA-256 authentifiziert alle Kopplungs- und Sitzungsnachrichten. Sitzungsnonce, Empfänger-/Sender-ID und streng steigende Sequenznummern verhindern die Wiederverwendung bereits angenommener Statusnachrichten innerhalb einer Sitzung.
- Das Protokoll verschlüsselt die Statuswerte **nicht**. WLAN-Verschlüsselung bleibt erforderlich. Den Kopplungsschlüssel nur direkt zwischen den vertrauenswürdigen Geräten übertragen.
- Schlüssel und Kopplungen liegen nur im RAM. Neustart, erkannter IP-/Subnetzwechsel und Verbindungsverlust setzen den Zustand zurück. Es werden keine Schlüssel in Konfigurationsdateien oder Logs geschrieben.
- Laufzeit, Paketgröße und Teilnehmerzahlen sind begrenzt. Das ist kein Schutz gegen Funkstörungen oder sämtliche Überlastungsangriffe. Gefälschte Discovery-Meldungen können die begrenzte Fundliste belegen.

## Prüfung und bekannte Grenzen

Der Host-Test `tests/device_status_protocol_test.cpp` prüft Serialisierung, Paketlängen, reservierte Felder, Nachrichtentypen, erlaubte Statuswerte, Hex-Schlüssel, Sitzungszuordnung, Replay-Vergleiche und Tag-Vergleiche. Er lässt sich mit einem C++17-Compiler und Include-Pfad `src/core/connect` ausführen; im Codespace wurde er zusätzlich mit AddressSanitizer und UndefinedBehaviorSanitizer ausgeführt.

Die Statusdateien wurden mit den tatsächlichen PlatformIO-Compileroptionen geprüft. Der vollständige Firmware-Build für das Zielboard **CYD-2USB** (ESP32, Standard-Umgebung in `platformio.ini`) ist erfolgreich: RAM 31,2 %, Flash 85,7 %. Die fertige Datei liegt im Projektstamm als `Bruce-CYD-2USB.bin` (Bootloader, Partitionen und Firmware zusammengeführt) und wird per Web-Flasher oder esptool an Offset `0x0` geflasht. Ein früherer Linkfehler (`-lnet80211`) betraf nur die Cardputer-Umgebung in diesem Codespace, weil dort `esp32s3/lib/libnet80211.a` nur als `.old`-Datei vorlag; er steht in keinem Zusammenhang mit dieser Änderung.

Ein erfolgreicher Protokolltest ist **kein** Ende-zu-Ende-Test der HMAC-Implementierung, WLAN-Übertragung, Pairing-Zustandsmaschine oder Oberfläche. Ein Hardwaretest mit zwei Geräten steht noch aus. Diese Änderung ergänzt weder eine JC-ESP32P4-M3-Portierung noch eine CrowPanel-7-Zoll-Displayansteuerung. Die Bedienoberfläche ist ein Gerätemenü, kein zusätzlicher Web-Tab.

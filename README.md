# CalendarBoard

ESP8266-Firmware für eine Tagesansicht auf hintereinandergeschalteten LED-Zeilen.
Jede Zeile hat 31 LEDs: Positionen 1–24 zeigen die Stunden 00–23, Position 25
zeigt Ganztagstermine. Die Positionen 26–31 bleiben vorerst schwarz. Normale
Termine erscheinen weiß, wichtige Termine (`Wichtig:` im Titel) rot.
VirtualEntities berechnet die Tagesprojektion.

## Einrichtung

`src/secrets.h` mit den lokalen Zugangsdaten anlegen (wird nicht versioniert):

```cpp
#define CALENDAR_BOARD_NAME "example-board"
#define MQTT_HOST "mqtt.example.local"
#define SECRET_MQTT_USER ""
#define SECRET_MQTT_PASS ""
#define SECRET_SSID ""
#define SECRET_PASS ""
#define OTA_PASS ""
```

Die Vorlage [BoardConfig.example.h](include/BoardConfig.example.h) nach
`include/BoardConfig.h` kopieren (ignoriert und ausschließlich lokal). Darin die Zeilen-IDs in physischer Reihenfolge
konfigurieren. Anzahl und LED-Puffer werden daraus abgeleitet. Die Beispiel-IDs
entsprechen der Vorlage. IDs und Boardname müssen mit der
`calendar_boards`-Konfiguration von VirtualEntities übereinstimmen.

Für eine andere Zeitzone sowohl `Timezone` (IANA-Name) als auch die dazugehörige
`TimezoneRule` (POSIX-Regel) ändern. Vorgabe: Europe/Berlin mit Sommer-/Winterzeit.
Damit arbeitet die Uhr auch ohne Verbindung mit dem richtigen Offset weiter.
Abweichende Zeitangaben des Senders werden zurückgewiesen. Die alte ESP8266-
Toolchain begrenzt den unterstützten Datumsbereich dieser Version auf 2000–2037.

PlatformIO verwendet `platformio.ini`; lokale Upload-Einstellungen können in der
ignorierten `upload_params.ini` stehen. Für einen frischen Checkout diese Datei
bei Bedarf leer anlegen. Vor einem späteren Upload dessen Ziel und OTA-Passwort
an die eigene Installation anpassen. Der folgende Befehl baut ausschließlich:

```sh
pio run -e ota
```

Die Plattform bleibt wegen der dokumentierten Flackerprobleme bei Version 2.6.3.
Der Build wurde mit dieser Version erfolgreich geprüft.

## MQTT und Verhalten

Die Firmware verwendet ausschließlich den neuen Datenpfad:

- `calendarboard/v1/{board_id}/time`: Datum/Uhrzeit atomar, nicht retained.
- `calendarboard/v1/{board_id}/rows/{row_id}/day`: vollständiger Tagesstand,
  retained, Subscription mit QoS 1.
- `calendarboard/v1/{board_id}/sync/request`: Anfrage nach dem aktuellen Stand
  bei Start, Wiederverbindung und lokalem Tageswechsel. Fehlende Antworten
  werden zunächst nach fünf Sekunden, anschließend alle 30 Sekunden nachgefragt.
- `home/things/{board_id}/nightmode`: weiterhin `on` beziehungsweise `off`.

Nachtmodus unterdrückt ausschließlich die Anzeige. Datenempfang und Uhr laufen
weiter. Vergangene belegte Stunden werden dunkelgrau. Die aktuelle Stunde wechselt
im Vier-Sekunden-Rhythmus zwischen einem warmen, gedimmten Ton und ihrer Terminfarbe.
Bei Verbindungsverlust bleibt der letzte Kalenderstand im RAM sichtbar. Ein alter
Tag bleibt bis zum Ersatz sichtbar, bekommt aber keine Stundenmarkierung des
neuen Tages. `unavailable` oder fehlerhafte Nachrichten löschen die Anzeige nicht.
Nach einem Neustart braucht das Board zunächst frische Zeit und passende Tagesdaten;
es gibt keine persistierte Uhr oder Terminablage.

Rendering ist vollständig und begrenzt LED-Ausgaben auf höchstens eine pro 200 ms,
ohne dafür MQTT oder OTA mit `delay()` anzuhalten. Der initiale WLAN-Verbindungsaufbau
verwendet weiterhin das bisherige Warte-/Neustartverhalten.

**Migration:** Vor einem Firmware-Upload den v1-Publisher in VirtualEntities für
passende IDs konfigurieren. Der alte `persons/.../appointments`-Pfad und die reine
`HH:MM:SS`-Nachricht werden von dieser Firmware nicht mehr verarbeitet. Die bisherige
Versorgung erst nach gemeinsamer Prüfung deaktivieren. Es wurde noch kein Board
geflasht und kein produktiver Sender umkonfiguriert.

## Prüfungen

Nach dem PlatformIO-Build sind die cJSON-Quellen für die Hosttests vorhanden:

```sh
sh test/native/run.sh
```

Die Tests laufen mit AddressSanitizer und UndefinedBehaviorSanitizer. Sie prüfen
Protokollvalidierung, unveränderten Zustand bei Fehlern, Nachtmodus, Uhrzeit- und
Tageswechsel, Sommer-/Winterzeit, `millis()`-Überlauf, Synchronisationswiederholungen
sowie LED-Indizes für 1, 4 und 6 Zeilen. `CJSON_DIR` kann auf ein anderes
Arduino_JSON-`src`-Verzeichnis zeigen. Die eingebundene Bibliothek erzeugt unter
macOS einige bestehende `sprintf`-Deprecation-Warnungen.

Die JSON-Fixtures stammen aus dem echten VirtualEntities-Publisher (Stand
`ec82d39`) mit gemocktem MQTT-Client. Zum Regenerieren eine Python-Umgebung mit
dessen Abhängigkeiten verwenden:

```sh
python test/generate_fixtures.py /pfad/zu/VirtualEntities
sh test/native/run.sh
```

Offen bleibt die Hardwareabnahme: Farben/Flackern, reale Zeilenverdrahtung,
MQTT-Reconnect und OTA im laufenden Betrieb. Hosttests und Build ersetzen diese
Prüfung nicht.

## Weiterentwicklung

- [Fehler und Board-Plan](docs/weiterentwicklungsplan.md)
- [VirtualEntities-Plan und gemeinsamer Nachrichtenvertrag](docs/virtualentities-weiterentwicklungsplan.md)

### Integration mit echtem Testbroker

Der Integrationstest startet einen temporären Broker ausschließlich auf
`127.0.0.1` mit einem freien Port. Er verwendet synthetische Kalender, den echten
VirtualEntities-Publisher und den unter Sanitizern gebauten Firmware-Kern.
Produktive Konfiguration und Zugangsdaten werden nicht eingelesen.

Eine separate Python-3.11-Umgebung mit `amqtt==0.12.0` vorbereiten. Den Test selbst
mit der Python-Umgebung von VirtualEntities ausführen:

```sh
python test/integration/run.py \
  --virtualentities /pfad/zu/VirtualEntities \
  --broker-python /pfad/zur/broker-venv/bin/python
```

Geprüft werden retained Tagesdaten ohne retained Uhrzeit, Antworten auf die
Board-Anfrage, fehlender Traffic bei unveränderten Daten, Nachtmodus,
Quellenfehler/-erholung, leere Kalender und Tageswechsel. Der Test startet zudem
den Broker neu und prüft automatische Wiederverbindung und erneuten Versand.
Er endet mit dem Stoppen des Testbrokers. Der ESP8266-Netzwerkstack und die
LED-Hardware sind nicht Teil dieses Hosttests.


Persönliche Zuordnungen, Serveradressen und Upload-Zugangsdaten dürfen nur in
ignorierten lokalen Konfigurationsdateien stehen. Versionierte Beispiele und Tests
verwenden ausschließlich synthetische Personen.

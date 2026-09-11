# Integration und Hardwareabnahme

Stand: 11. September 2026.

## Erfolgreich geprüft

- Firmware-Kern mit echten JSON-Nachrichten aus VirtualEntities: Protokoll,
  Uhrzeit, Rendering und Zeilenabbildung, unter AddressSanitizer und
  UndefinedBehaviorSanitizer.
- Firmware-Build für D1 mini / espressif8266 2.6.3.
- Echter isolierter MQTT-Broker (amqtt 0.12.0), produktiver Python-Publisher und
  nativer C++-Firmware-Kern in einer zusammenhängenden Prüfung:
  - Spät gestarteter Empfänger erhält vier retained Zeilen, aber keine alte Uhrzeit.
  - Synchronisationsanfrage liefert Zeit und alle Zeilen.
  - Unveränderte Kalender erzeugen keine zusätzlichen Tagesnachrichten.
  - Nachtmodus erhält Änderungen und zeigt sie beim Aufwachen.
  - Quellenfehler erhalten den letzten vollständigen Stand; Erholung und leere
    Kalender werden übertragen.
  - Offline-Tageswechsel erhält die Daten bis zur Antwort für den neuen Tag.
  - Neustart des Brokers führt automatisch zur Wiederverbindung und zum erneuten
    Versand der Zeit und aller Zeilen, auch ohne erhaltene Brokerdatenbank.

Testdateien: `test/integration/run.py`, `broker.py` und `board_receiver.cpp`.
Der native Adapter verwendet die echten Firmwareklassen; die ESP8266-Netzwerk-
und LED-Treiber müssen noch auf Hardware geprüft werden.

## Stand der Abnahme

Die v1-Firmware wurde erfolgreich per OTA installiert. Das Board meldet sich
danach wieder online; Zeit und vier Tageszeilen stehen am Broker bereit.
Sichtprüfung: Zeilenzuordnung, Stundenmarkierung sowie weiße und rote Termine
sind korrekt; im regulären Betrieb wurde kein störendes Flackern beobachtet.

Nachtmodus wurde sichtbar geprüft: LEDs aus, danach wieder korrekt angezeigt.
Ein kontrollierter MQTT-Reconnect lieferte zwei Synchronisationsanfragen,
Zeitnachrichten und alle vier Zeilen; das Board blieb wieder online.
Installationsdetails und persönliche Zuordnungen werden ausschließlich lokal
geführt.

## Vor dem Upload erledigt

1. Die tatsächlich vom Service geladene YAML-Datei feststellen.
2. `calendar_boards` ergänzen oder abgleichen. `person` muss genau dem jeweiligen
   vorhandenen Personennamen entsprechen; die IDs bleiben klein geschrieben:

   ```yaml
   calendar_boards:
     - id: example-board
       timezone: Europe/Berlin
       rows:
         - {id: person1, person: person1}
         - {id: person2, person: person2}
         - {id: person3, person: person3}
         - {id: person4, person: person4}
   ```

3. Service neu starten und auf Konfigurations-/Importfehler prüfen. Die installierte
   Version muss den Publisher-Commit `ec82d39` oder einen Nachfolger enthalten.
4. Erneut `/sync/request` senden. Frische Zeit für `Europe/Berlin` und vier zum
   aktuellen Tag passende Zeilen mit `status: ok` müssen eintreffen. Ein leerer
   Kalender mit 24 null-Werten ist ein gültiges Ergebnis.
5. Firmware hochgeladen und grundlegende Hardwareprüfung durchgeführt. Die alte
   Datenversorgung bleibt bis zur vollständigen Abnahme bestehen; die vorherige
   Firmware liegt lokal als Rückweg vor. Zugangsdaten und reale Termine gehören
   nicht ins Testprotokoll.

## Hardwareprüfung – noch offen

- [x] Boot mit frischer Zeit und allen vier Zeilen.
- [x] Physische Zuordnung, Weiß/Rot und Stundenmarkierung.
- [x] Keine störenden Flackereffekte im regulären Betrieb nach dem Update.
- [x] Nachtmodus und Aufwachen mit sichtbarer Bestätigung.
- [x] Unterbrochene Verbindung und Wiederverbindung auf echter Hardware.
- [ ] Leere Terminliste auf echter Hardware.
- [ ] Lokaler Tageswechsel und Ersatz durch den neuen Tagesstand.
- [x] OTA weiterhin erreichbar nach der Umstellung.


## Beispielkonfiguration und Abrufintervalle

Das Fragment in [virtualentities-board.yaml](../config/virtualentities-board.yaml)
verwendet ausschließlich synthetische Personen. Die tatsächlichen Zuordnungen
werden nur in der lokalen Serverkonfiguration eingetragen.

Für einen 15-Minuten-Takt beispielsweise `*/15 * * * * 15` verwenden. Bei der
hier eingesetzten croniter-Konvention steht das Sekundenfeld am Ende. Zwischen
Abrufen dürfen bei der aktuellen Publisher-Konfiguration höchstens 30 Minuten
liegen, damit die Daten nicht als veraltet gelten.

Die alten Benachrichtigungen können bis zur Hardwareabnahme bestehen bleiben.
Die Board-Projektion verwendet ausschließlich das Titelpräfix `Wichtig:` für Rot.

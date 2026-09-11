# VirtualEntities: Kalenderboard-Anbindung und Weiterentwicklungsplan

Stand: 11. September 2026. Analysierter Stand: VirtualEntities `3679307`, lokales Projekt `/path/to/VirtualEntities`.

Dieses Dokument ergänzt den [CalendarBoard-Plan](weiterentwicklungsplan.md). Es liegt zunächst gemeinsam mit diesem im CalendarBoard-Repository und dient beiden Projekten als Schnittstellenreferenz. Die folgenden Protokoll- und Fachregeln sind ein konkreter Umsetzungsvorschlag, noch kein implementiertes Verhalten.

## 1. Ziel und Umfang

VirtualEntities ersetzt OpenHABian als Lieferant für Kalenderdaten und Zeitinformationen. Es lädt die Kalender der Personen bereits selbst. Deshalb können Normalisierung, Auswahl und Konfliktregeln zentral und automatisiert geprüft werden.

VirtualEntities liefert Datum, Uhrzeit und eine Tagesprojektion je konfigurierter Kalenderzeile. Das Board bleibt bei der Tagesansicht mit 24 Stundenpositionen auf physisch 31 LEDs pro Zeile. Monatsansicht und Laufzeitänderungen der Zeilenzahl gehören nicht zu dieser Umsetzung.

Die Analyse umfasst insbesondere Personen, Kalender, CalDAV-Import, MQTT-Versand, deren Initialisierung und vorhandene Tests. Sie ist keine vollständige Prüfung der Geräte-, Raum- oder Webfunktionen des Gesamtprojekts. Bei der ursprünglichen Analyse fehlten Python-Abhängigkeiten. Für die erste Umsetzung wurde inzwischen eine isolierte Testumgebung eingerichtet; siehe Fortschritt.

## 2. Bestehender Datenfluss und Bewertung

`main.py` erzeugt pro Person einen `PersonService` und `MqttPersonMediator`. Der Mediator lädt je Kalenderquelle in einem Thread ungefähr sieben Tage über CalDAV, einschließlich angeforderter Wiederholungsauflösung. `CalendarLoader` wandelt die Ereignisse in `Appointment` um und bestimmt Farben anhand von Kategorien beziehungsweise Kalenderfarben. `PersonService` ersetzt die jeweilige Kalenderquelle im Personenregister.

Zusätzlich sendet der Mediator über konfigurierte Cron-Benachrichtigungen mit dem Subject `daily-appointments` eine Tagesliste. Das Ausgabeformat besteht aus `appointments` mit Titel, Start, Ende, Farbe und Aktualisierungszeit. Ein eigener Datum-/Zeitpublisher ist in diesem Pfad nicht vorhanden. Erfolgreicher Import und Versand sind zeitlich unabhängig.

Gut nutzbare Grundlagen sind die vorhandenen Personen- und Kalenderobjekte, der separate Loader, konfigurierbare Quellen und Ziele sowie Unit- und Verhaltenstests. Für die neue Aufgabe vermischt der Personen-MQTT-Mediator allerdings CalDAV, Scheduling, Tagesauswahl und MQTT. Die unter `iot/infrastructure` abgelegte Fachlogik ist zudem an Systemzeit und feste Zeitzonen gekoppelt. Vor allem Verantwortlichkeiten und Zeitinvarianten sollten verbessert werden; eine umfassende Umbenennung aller Verzeichnisse ist dafür nicht nötig.

## 3. Befunde

Die Pfade in dieser Tabelle beziehen sich auf VirtualEntities. P1 bedeutet vor produktiver Board-Anbindung beheben, P2 anschließend im betroffenen Umbau.

| ID | Priorität | Befund | Quelle |
| --- | --- | --- | --- |
| VE01 | P1 | `Appointment` akzeptiert laut Typ `date`, ruft aber immer `.astimezone()` auf. Ganztägige Ereignisse mit Datumswerten verursachen `AttributeError` und werden vom Loader übersprungen. | `iot/infrastructure/time/calendar.py`, `Appointment.__init__`; `iot/dav/calendar_reader.py` |
| VE02 | P1 | `covers_interval()` behandelt beide Grenzen inklusiv. Ein exakt am Stundenanfang endender Termin zählt dadurch noch in diese Stunde; dies widerspricht der strengeren Überlappungsprüfung des Boards. | `iot/infrastructure/time/calendar.py` |
| VE03 | P1 | Host-Zeitzone, naive Mitternachtswerte und festes `Europe/Berlin` werden gemischt. Das Tagesfenster endet bei 23:59:59 statt exklusiv am nächsten Tagesbeginn. Verhalten kann vom Serverstandort abhängen und Randfälle auslassen. | `iot/mqtt/mqtt_person_mediator.py`, `_get_appointments_for_today`, `_update_calendars_from_caldav`; Kalenderklassen |
| VE04 | P1 | Pro Quelle laufende Threads aktualisieren Personen über ungeschütztes Lesen–Ändern–Ersetzen. Zwei gleichzeitige Quellenupdates können jeweils den alten Stand der anderen Quelle zurückschreiben. | `iot/infrastructure/person_service.py`, `update_calendars`; `iot/infrastructure/register_of_persons.py` |
| VE05 | P1 | `DTEND` und `SUMMARY` werden ungeprüft gelesen. Ein fehlendes Feld kann den gesamten Quellenlauf abbrechen; nur `AttributeError` wird pro Ereignis abgefangen. Teilweise übersprungene Ereignisse können andererseits als scheinbar vollständiges Ergebnis übernommen werden. | `iot/dav/calendar_reader.py` |
| VE06 | P1 | Unbekannte, noch nicht geladene und tatsächlich leere Kalender sind im Tagespayload nicht unterscheidbar. Quellenfehler behalten alte Daten, ohne deren Zustand im Board-Payload kenntlich zu machen. | Personenservice und Personen-MQTT-Mediator |
| VE07 | P1 | Der MQTT-Wrapper bietet weder QoS-/Retain-Parameter noch Auswertung des Publish-Ergebnisses. Es gibt keinen expliziten Wiederanlauf des Board-Snapshots; Versand erfolgt nach Cron statt unmittelbar nach Datenänderungen. | `iot/mqtt/mqtt_client.py`, `publish`; `iot/mqtt/mqtt_mediator.py` |
| VE08 | P2 | `Calendar` verwendet eine gemeinsam genutzte Defaultliste; mehrere Konstruktoren berechnen Default-Zeitstempel beim Import statt bei der Instanzerzeugung. | `iot/infrastructure/time/calendar.py`; `iot/infrastructure/person.py` |
| VE09 | P2 | Scheduler laufen endlos ohne Stoppsignal. `shutdown()` joint gerade nicht laufende Threads; `MqttClient.stop()` beendet den Netzwerkloop nicht aktiv. Die spezielle CalDAV-Threadliste bleibt leer, da Threads in der Basisklassenliste landen; gestartet werden sie dennoch durch die Basisklasse. | `iot/mqtt/mqtt_mediator.py`; `mqtt_person_mediator.py`; `mqtt_client.py` |
| VE10 | P2 | Personen und Kalender werden über veränderliche Namen identifiziert. Termine behalten keine Quell-UID oder Wiederholungsidentität. Stabile Zuordnung und deterministische Konfliktauflösung sind damit erschwert. | Personenregister, Personenservice, Kalender und Loader |
| VE11 | P2 | `get_n_upcoming_appointments()` sortiert alle geladenen Termine, filtert aber vergangene nicht aus. | `iot/infrastructure/person.py` |

## 4. Verantwortlichkeiten im Ziel

- **CalDAV-Adapter:** Quellen lesen, Wiederholungen und Ausnahmen berücksichtigen, Datums- und Zeittypen korrekt normalisieren; Fehler und Ladezustand liefern.
- **Kalender-Anwendungsdienst:** Quellenzustände atomar aktualisieren; erfolgreiche Änderungen und Tageswechsel als Anlass für neue Projektionen behandeln.
- **Tagesprojektion:** Termine einer Person oder eines Kalenders in genau 24 logische Stundenbelegungen umrechnen. Keine MQTT-Aufrufe und keine physischen LED-Indizes.
- **Board-Publisher:** Projektionen als versionierte Snapshots ausgeben; Zustellung, Wiederverbindung und Abfragen koordinieren.
- **Zeitdienst:** Eine injizierbare Uhr verwenden und Zeitnachrichten für die konfigurierte Board-Zeitzone erzeugen.
- **CalendarBoard:** Nachrichten prüfen, Zeilen zuordnen, lokale Uhr fortschreiben und Farben einschließlich Blinkmarkierung, Vergangenheitsdarstellung und Nachtmodus rendern.

VirtualEntities behält vollständige Termine für seine Website. Die Board-Schnittstelle verwendet eine eigene kompakte Projektion und nicht unmittelbar `Appointment.to_dict()`. Das reduziert Payload und Firmware-Komplexität und verhindert die Übertragung unnötiger Titel, Beschreibungen und Kalender-URLs.

## 5. Vorgeschlagene fachliche Regeln

1. Eine Board-Konfiguration bestimmt die Zeitzone; Vorgabe ist `Europe/Berlin`. Die Server-Zeitzone hat keinen Einfluss. Zeitgebundene Termine werden als eindeutige Zeitpunkte verarbeitet. Zeiten ohne Offset erhalten ausdrücklich die konfigurierte Quellenzeitzone.
2. Intervalle sind halboffen: `[Beginn, Ende)`. Eine Überschneidung liegt vor, wenn `Terminbeginn < Fensterende` und `Terminende > Fensterbeginn`. Negative Zeitintervalle werden zurückgewiesen. Termine ohne Dauer bleiben für andere Verbraucher erhalten, belegen aber keine Stundenposition.
3. Ganztägige Termine bleiben Datumsintervalle mit exklusivem Enddatum. Bei der Tagesprojektion belegen sie keine Stunden, sondern setzen `all_day` für LED 25: weiß für normale, rot für wichtige Termine. Fehlendes Ende, `DURATION` und unvollständige Quellenereignisse erhalten einen dokumentierten Importpfad mit eigenen Testfällen; keine stillschweigende Umwandlung in leere Daten.
4. Mehrtägige und über Mitternacht laufende Termine werden auf den jeweiligen lokalen Tag begrenzt. Wiederholungen, Ausnahmen und gelöschte Instanzen müssen im Ergebnis korrekt enthalten beziehungsweise entfernt sein.
5. Eine Stunde ist belegt, sobald ein Termin sie zeitlich überschneidet. Normale Termine erscheinen weiß (`ffffff`). Beginnt der Titel exakt mit `Wichtig:`, erscheint der Termin rot (`ff0000`). Bei Überschneidungen gewinnt Rot; Kalenderprioritäten und zusätzliche Farbtöne entfallen.
6. Farben sind sechs kleingeschriebene Hexziffern ohne `#`. Für das Board wird die Farbe ausschließlich aus dem Titelpräfix abgeleitet. Bestehende Kalender- und Kategoriefarben anderer Verbraucher bleiben davon unabhängig.
7. Die Tagesansicht besitzt auch an Zeitumstellungstagen 24 Positionen: Eine ausgefallene lokale Stunde ist unbelegt; beide Vorkommen einer wiederholten Stunde werden auf derselben Position zusammengeführt. Überschneidungen werden auf der realen Zeitachse berechnet, anschließend lokalen Stunden zugeordnet.
8. Jede Zeile umfasst alle Kalender einer konfigurierten Person. Ganztagstermine setzen `all_day` für LED 25; die LEDs 26–31 bleiben in dieser Version schwarz. Nachtmodus und aktuelle Stunde sind Darstellung des Boards und verändern keine empfangene Tagesprojektion.

## 6. MQTT-Vertrag v1: Entwurf

Neuer, separater Topic-Zweig: `calendarboard/v1/{board_id}/...`. Der bestehende OpenHAB-Pfad bleibt während der Migration getrennt. IDs sind stabile, konfigurierte ASCII-Topicsegmente aus Buchstaben, Ziffern, `_` und `-`, maximal 32 Zeichen. Jede logische Zeile verweist auf eine Person oder einen einzelnen Kalender; das Board ordnet die `row_id` seinem physischen Zeilenindex zu.

### Tagesdaten: `rows/{row_id}/day`

QoS 1, retained, vollständiger Ersatz genau einer Zeile und eines Tages. Keine Deltas. Beispiel für eine erfolgreich geladene, terminfreie Zeile:

```json
{
  "schema_version": 1,
  "date": "2026-09-11",
  "timezone": "Europe/Berlin",
  "generated_at": "2026-09-11T12:00:00Z",
  "source_checked_at": "2026-09-11T11:59:50Z",
  "status": "ok",
  "all_day": null,
  "slots": [null, null, null, null, null, null, null, null,
            null, null, null, null, null, null, null, null,
            null, null, null, null, null, null, null, null]
}
```

`slots` enthält genau 24 Einträge, Index 0 entspricht 00 Uhr. Ein Eintrag ist `null` für unbelegt oder beispielsweise `"ff0000"` für einen belegten Slot. `all_day` ist `null`, `"ffffff"` oder `"ff0000"` und steuert LED 25. Schwarz als Terminfarbe ist damit weiterhin von unbelegt unterscheidbar. Vergangenheitsgrau wird nicht übertragen.

`status` ist `ok`, `stale` oder `unavailable`. `source_checked_at` ist der älteste erfolgreiche Prüfzeitpunkt der benötigten Quellen, bei fehlender Erstladung `null`. Bei einem Quellenfehler darf die letzte vollständige Projektion desselben Tages als `stale` wiederholt werden. Ohne vollständigen Stand für diesen Tag gilt `unavailable` und `slots: null`. Keine irreführenden Teilprojektionen als `ok` veröffentlichen.

Das Board akzeptiert Tagesdaten nur für sein synchronisiertes lokales Datum und seine konfigurierte Zeitzone. Vor der Zeitsynchronisation darf es einen Snapshot zwischenspeichern, aber nicht als aktuellen Tag anzeigen. Alte Datumsstände werden nicht als neuer aktueller Snapshot übernommen; bei ausbleibender Antwort bleibt die letzte Anzeige gemäß Offline-Regel erhalten. Bei `unavailable` bleibt die letzte Anzeige erhalten; Diagnose erfolgt über Status/Logs. Eine `stale`-Projektion desselben Tages darf sichtbar bleiben. Ungültige Nachrichten erhalten den letzten gültigen Zustand.

### Datum und Zeit: `time`

QoS 0, **nicht retained**, alle 30 Sekunden, nach Verbindung und als Antwort auf `sync/request`:

```json
{
  "schema_version": 1,
  "utc": "2026-09-11T12:00:00Z",
  "local": "2026-09-11T14:00:00+02:00",
  "timezone": "Europe/Berlin"
}
```

Datum und Uhrzeit werden gemeinsam und atomar übernommen. UTC und lokale Zeit müssen denselben Zeitpunkt bezeichnen. Das Board schreibt die empfangene Zeit mit einer monotonen Laufzeituhr fort. Bei Verbindungsverlust bleibt die Anzeige erhalten und die Uhr läuft lokal weiter. Am lokalen Tageswechsel fragt das Board den neuen Tag an; bis zur Antwort bleibt der letzte Stand sichtbar und wird intern als veraltet geführt. Offsetwechsel ohne Verbindung benötigen eine gesonderte Firmware-Regel; automatisches Dunkelwerden entfällt. Dies ist eine grobe Anzeigeuhr, kein präziser Zeitabgleich gegen Netzwerklatenz.

### Synchronisation und Versand

- Board sendet nach jeder Verbindung `sync/request` mit `{"schema_version":1}`; nicht retained, QoS 0. Ohne Antwort Wiederholung nach fünf Sekunden, danach alle 30 Sekunden bis eine gültige Zeit vorliegt.
- VirtualEntities antwortet mit frischer Zeit und den verfügbaren Tages-Snapshots. Das aktuelle MQTT-Callback darf dafür keine blockierenden CalDAV-Abfragen ausführen.
- Neue Snapshots entstehen nach erfolgreichem Quellenupdate, Änderung der Zeilenzuordnung, lokalem Tageswechsel und Publisher-Reconnect. Unveränderte Projektionen werden nicht periodisch wiederholt. Versand erfolgt bei Änderung der Slots, des Tages oder des Quellenstatus sowie auf Anfrage. Zeitkorrekturen bleiben davon unabhängig periodisch. Datenberechnung und Snapshot-Erzeugung werden je Board serialisiert, damit keine älteren Jobs neuere Ergebnisse überschreiben.
- Retained Snapshots werden beim Entfernen einer Zeile explizit gelöscht. Genau ein aktiver Publisher ist pro Board zuständig. QoS-1-Duplikate werden idempotent verarbeitet.
- Maximal 768 UTF-8-Bytes pro Payload; Topic einschließlich maximaler IDs höchstens 96 Bytes. Sender prüft vor Versand, Empfänger prüft vor Übernahme. Das tatsächliche MQTT-Paket muss einschließlich Header in den Firmware-Empfangspuffer passen und wird mit maximal gefüllten Slots getestet.
- Nachtmodus bleibt zunächst eine eigenständige Board-Funktion über den bestehenden Steuerpfad. Der Wechsel des Kalendersenders definiert noch keinen neuen Lieferanten für diesen Befehl.

## 7. Umsetzungsetappen und Abnahme

### V1: Zeit- und Importmodell stabilisieren

- [ ] VE01–VE03, VE05 und VE08 beheben.
- [ ] Stabile Quellen-/Terminidentität ergänzen und Zeitzonen explizit konfigurieren (VE10).
- [ ] Regeln aus Abschnitt 5 mit festen Zeitwerten testen.

**Abnahme:** Ganztägige Termine verschwinden nicht; angrenzende Termine überlappen nicht; Mitternacht und beide Zeitumstellungen funktionieren unabhängig von der Host-Zeitzone. Einzelne Importfehler sind sichtbar und erzeugen keinen scheinbar vollständigen leeren Kalender.

### V2: Aktualisierung und Quellenzustand entkoppeln

- [x] CalDAV-Scheduling aus `MqttPersonMediator` ausgelagert (`CalendarSync`).
- [x] Quellenstände im Personenservice unter einer Sperre aktualisieren (VE04).
- [x] Erstladung, erfolgreich leer, Fehler und veralteten Stand im Publisher unterscheiden (VE06).
- [ ] Stoppsignal und unterbrechbare Wartezeiten für Scheduler und MQTT ergänzen (VE09).

**Abnahme:** Zwei gleichzeitig aktualisierte Kalender verlieren keine Daten. Ausfall einer Quelle wird erkannt. Shutdown beendet laufende Aufgaben kontrolliert. Webansicht und Board-Projektion lesen einen konsistenten Stand.

### V3: Reine Tagesprojektion und Vertragsfixtures

- [ ] 24 Slots gemäß Abschnitt 5 berechnen, ohne Netzwerkzugriff.
- [ ] Konfiguration für Board-ID, Zeitzone und stabile Zeilenzuordnungen ergänzen.
- [ ] Maschinenlesbare Beispiele für beide Projekte erstellen: leer, belegt, Konflikt, ganztägig, Mitternacht, Sommer-/Winterzeit, stale und unavailable.
- [ ] Maximale Nachrichtengröße automatisch prüfen.

**Abnahme:** Identische Eingaben erzeugen unabhängig von Importreihenfolge dieselbe Projektion. Mehrere Kalender einer Person und unterschiedliche Zeilenzahlen sind abgedeckt. Die Website behält ihre vollständigen Termindaten.

### V4: Publisher und Zeitdienst

- [x] MQTT-Wrapper um QoS, Retain und überprüfbare Versandresultate erweitert (VE07).
- [x] Snapshot- und Zeitpublisher sowie Synchronisationsanfrage implementiert und im Anwendungsstart verdrahtet.
- [ ] Tageswechsel, Wiederverbindung, periodische Wiederholung und gelöschte Zeilen behandeln.
- [ ] Mit Testbroker Zustellung, Duplikate und retained Daten prüfen.

**Abnahme:** Ein später gestartetes Board erhält frische Zeit und Tagesdaten. Ein Broker-Neustart erfordert keinen Neustart der Anwendung. Alte retained Daten stellen weder die Uhr zurück noch werden sie am falschen Tag angezeigt.

### V5: Board umstellen und gemeinsam abnehmen

- [x] CalendarBoard um v1-Validierung, atomare Datum-/Zeitübernahme und Slot-Empfang ergänzt.
- [x] Physisches Mapping und vollständiges Rendering implementiert und nativ geprüft; Verdrahtung/Flackern auf Hardware noch prüfen.
- [ ] Beide Sender zunächst auf getrennten Topics vergleichen; niemals beide auf denselben produktiven Datenpfad schreiben lassen.
- [ ] Testboard auf v1 umstellen und auf Hardware prüfen; danach produktive Umstellung und alte OpenHAB-Regeln deaktivieren.
- [ ] Alten Topic-Pfad als dokumentierten Rückweg bis zur erfolgreichen Abnahme erhalten.

**Abnahme:** Boot, MQTT-Unterbrechung, Tageswechsel, Uhrzeitsprung, leere Termine und Nachtmodus funktionieren Ende zu Ende. Die Board-Firmware benötigt keine CalDAV-, Zeitzonen- oder Termin-Konfliktlogik. Monatsansicht bleibt außerhalb des Umfangs.

### V6: Ergänzende Bereinigung

- [ ] VE11 beheben und entsprechende Web-/Personentests ergänzen.
- [ ] Verbliebene globale Zeitabhängigkeiten und veränderliche Defaultwerte im angefassten Bereich bereinigen.
- [ ] Einrichtung, Board-Konfiguration und Diagnose dokumentieren.

## 8. Fortschritt

| Datum | Ergebnis | Prüfung | Offen |
| --- | --- | --- | --- |
| 2026-09-11 | Kalender-/MQTT-Pfad analysiert; Zielaufteilung, Protokollentwurf und Etappen dokumentiert | Statische Analyse; ausgewählte Tests wegen fehlender Abhängigkeiten nicht ausführbar | V1–V6, Vertragsfixtures und gemeinsame Hardwareabnahme |

### Erste Umsetzung: Zeitmodell und Tagesprojektion

- [x] VE01: Datumswerte ganztägiger Termine erhalten; Board-Projektion ignoriert diese.
- [x] VE02: Halboffene Intervallgrenzen und Termine ohne Dauer geprüft.
- [x] VE08: Gemeinsame Defaultliste und vorzeitig berechnete Zeitstempel in Calendar/Person korrigiert.
- [x] Reine Funktion `project_day()` ergänzt: 24 Slots, Weiß/Rot nach Titelpräfix, Rot gewinnt, Mitternacht und Zeitumstellungen.
- [x] Konfigurierbare Zeitzone als Funktions-/Konstruktorparameter, Standard Europe/Berlin. Naive Terminzeiten hängen nicht mehr von der Host-Zeitzone ab.
- [x] Sortierung gemischter Datums- und Zeitwerte in der Personenansicht abgesichert.
- [ ] Zeitzonenparameter an Quellen-/Board-Konfiguration anschließen; VE03 im bisherigen MQTT-Pfad bleibt offen.
- [x] Tagesprojektion in einen optionalen MQTT-Publisher integriert. Ohne Board-Konfiguration bleibt ausschließlich der alte Pfad aktiv.

Prüfung: 20 Tests erfolgreich (acht neue Tests plus bestehende Kalender-, Loader- und Personentests), auch nach Übernahme ins VirtualEntities-Repository. `git diff --check` ohne Befund. Keine Hardware-/Brokerabnahme. Null-Dauer-Termine bleiben aus Kompatibilitätsgründen erhalten, erzeugen aber keine Stundenbelegung. Weitere Importfälle und Quellenzustände aus V1/V2 bleiben offen.


### Umsetzung: Quellenaktualisierung und MQTT-Publisher

VirtualEntities-Commits:

- `366a1fa`: Tagesprojektion und korrigierte Zeitintervalle (erster Implementierungsblock).
- `e436eb9`: Atomare Quellenupdates, Fehlerzustand, eigener CalDAV-Lebenszyklus und unterbrechbare Scheduler-Wartezeiten. Referenzierte Kalender werden nun ebenfalls berücksichtigt. Fehlende Enden erhalten je nach Datum/Dauer einen definierten Importpfad; ein fehlerhaftes Ereignis verwirft den gesamten neuen Quellenstand.
- `ec82d39`: Optionaler Board-Publisher mit YAML-Konfiguration und Integration in `main.py`, Zeitnachrichten, Tages-Snapshots, Synchronisationsanfragen und Wiederverbindung. QoS-1-Snapshots werden erst nach Bestätigung als gesendet gemerkt; pro Topic bleibt höchstens ein nicht bestätigter Publisher-Auftrag offen.

Prüfung: Alle **204 Unit-Tests** in der isolierten Arbeitskopie erfolgreich, einschließlich Quellenfehlern, Scheduler-Shutdown, Konfigurations-Roundtrip, Tageswechsel, unveränderten Daten, Größenlimit, Wiederverbindung und ausbleibenden MQTT-Bestätigungen. Der geprüfte Stand wurde unverändert ins Zielrepository übernommen. Kein Live-Broker-/Hardwaretest und keine produktive Konfiguration geändert.

Betriebsdokumentation im VirtualEntities-Repository: `docs/calendarboard.md`. Quelle und Board haben separat konfigurierbare Zeitzonen, jeweils Standard `Europe/Berlin`. Kalenderdaten werden im Speicher einmal pro Sekunde auf relevante Änderungen geprüft; unveränderte Abrufzeitstempel erzeugen keine MQTT-Nachricht.

Verbleibende Grenzen:

- Die Board-Firmware konsumiert jetzt v1 und wurde erfolgreich gebaut sowie mit Sender-Fixtures getestet. Produktive Umstellung und Broker-/Hardwareabnahme aus V5 bleiben offen.
- Gelöschte Zeilen müssen vorerst administrativ aus dem retained Brokerbestand entfernt werden. Automatische Bereinigung ist noch nicht implementiert.
- Bei einem Neustart des Publishers existiert kein persistierter letzter vollständiger Stand. Bis frische Quellen geladen sind, meldet er `unavailable`; das Board soll seine letzte Anzeige behalten.
- Quellen gelten nach 30 Minuten ohne erfolgreichen Abruf als veraltet. Die produktiven Abrufintervalle müssen dazu passen.
- Stoppsignale beenden Scheduler-Wartezeiten; ein bereits laufender CalDAV-Netzwerkaufruf kann noch bis zum konfigurierten Timeout laufen.
- Wiederholungs-/Ausnahmefälle des CalDAV-Servers, Offsetwechsel einer offline laufenden Board-Uhr und unterschiedliche Quellen-/Board-Zeitzonen benötigen weitere Ende-zu-Ende-Prüfungen.

### Gemeinsame Brokerprüfung

Der echte lokale Brokertest zwischen Publisher und nativem Firmware-Kern ist
abgeschlossen, einschließlich retained Daten, Synchronisationsanfrage,
Änderungsversand, Quellenfehlern, Tageswechsel und Broker-Neustart. Auf dem
produktiven Broker antwortet bisher noch kein v1-Publisher. Der Nutzer hat Pull
und Neustart des Server-Service bestätigt; die geladene Kalender-/Boardkonfiguration
muss noch abgeglichen werden. Siehe [Abnahmeprotokoll](integration-abnahme.md).


### Architekturentscheidung: allgemeine Ziele und Board-Protokoll

Beide Ausgabewege bleiben bestehen:

- Personenbezogene `destinations` veröffentlichen allgemeine Termindaten für
  beliebige Verbraucher.
- `calendar_boards` konfiguriert das spezifische Board-Protokoll mit
  Tagesprojektionen, Zeilenzuordnung, Zeitzone und Synchronisationsanfragen.

Der Board-Publisher wird nicht in `destinations` integriert. Die getrennten
Konfigurationen beschreiben unterschiedliche Ausgaben desselben Kalenderbestands;
die Quellen werden dafür nicht doppelt geladen. Konkrete Personen- und
Gerätezuordnungen bleiben ausschließlich in der lokalen Serverkonfiguration.

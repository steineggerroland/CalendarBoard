# CalendarBoard: Fehler und Weiterentwicklungsplan

Stand: 11. September 2026

Bestätigte Regeln: Normale Termine weiß, Titelpräfix `Wichtig:` rot; Rot gewinnt bei Überlappungen. Ganztägige Termine erscheinen auf LED 25, ohne Stundenpositionen zu belegen. Bei Verbindungsverlust bleiben Anzeige und lokale Uhr aktiv. Tagesdaten werden bei Änderungen und auf Anfrage (Start/Tageswechsel) gesendet. Zeitzone konfigurierbar, Standard Europe/Berlin. Die Zeilen werden lokal über stabile IDs konfiguriert. LEDs 26–31 bleiben frei für spätere Tagesinformationen. Diese Entscheidungen ersetzen abweichende offene Vorschläge unten.

Ergänzung: VirtualEntities übernimmt künftig Kalenderdaten sowie Datum und Uhrzeit von OpenHABian. Der [VirtualEntities-Plan mit gemeinsamem MQTT-Vertragsentwurf](virtualentities-weiterentwicklungsplan.md) konkretisiert die offenen Regeln dieses Dokuments. Vorgesehen ist eine serverseitige Projektion auf 24 Stundenbelegungen; das Board übernimmt Validierung, Zeitfortschreibung und Darstellung. Die dort vorgeschlagenen Regeln sind noch nicht implementiert. Insbesondere entfällt nach der Umstellung die bisher vorgesehene Terminintervallberechnung in der Firmware; entsprechende Fachtests werden in VirtualEntities umgesetzt, die Firmware prüft die gemeinsamen Vertragsfixtures.

Dieses Dokument ist der gemeinsame rote Faden für die Stabilisierung und Weiterentwicklung. Es hält fachliche Entscheidungen, bekannte Fehler und die Reihenfolge der Arbeiten fest. Erledigte Punkte werden mit ihrer Prüfung dokumentiert; neue Erkenntnisse werden hier ergänzt.

Die Bestandsaufnahme beruht auf einer statischen Quellcodeanalyse. Die Befunde wurden noch nicht durch einen Firmware-Build oder Hardwaretests geprüft. Die Umsetzung der folgenden Aufgaben steht aus.

## 1. Fachliche Leitplanken

- Das Board zeigt mehrere Kalender an. Jede Zeile repräsentiert eine Person beziehungsweise einen Kalender.
- Jede physische Zeile besitzt genau **31 LEDs**. Alle Zeilen bilden eine hintereinandergeschaltete LED-Kette.
- Unterschiedliche Boards sollen unterschiedlich viele Zeilen unterstützen.
- Termine werden über MQTT übermittelt.
- Aktuell bleibt ausschließlich die **Tagesansicht mit 24 Stundenpositionen** implementiert.
- Weitere Ansichten, insbesondere eine Monatsansicht, sind eine spätere Erweiterung. Sie werden jetzt nicht umgesetzt.
- Die physische Zeilenlänge von 31 LEDs und die 24 Stundenpositionen der Tagesansicht sind unterschiedliche Konzepte und werden getrennt modelliert.

## 2. Bekannte Fehler

Prioritäten: **P1** = zuerst beheben, da Stabilität oder wesentliche Funktionen betroffen sind; **P2** = anschließend für korrekte Darstellung und Zeitverarbeitung; **P3** = kleinere Korrektur.

| ID | Priorität | Befund und Auswirkung | Quellcode |
| --- | --- | --- | --- |
| F01 | P1 | `"event is over" + startIndex` führt Zeigerarithmetik statt Stringverkettung aus. Die Offsets 31, 62 und 93 liegen außerhalb des Stringliterals; undefiniertes Verhalten ist möglich. | `src/main.cpp`, `messageHandler()` |
| F02 | P1 | Der Nachtmodus löscht den Puffer, wird aber beim periodischen Blinken nicht berücksichtigt. Das Board bleibt nicht dunkel. | `src/main.cpp`, `messageHandler()`, `blinkEverySecond()` |
| F03 | P1 | Beim Verlassen des Nachtmodus erfolgt keine vollständige Neudarstellung. Identische erneut empfangene Termine werden wegen `if (!changed) return` nicht angezeigt. Das Abbestellen während der Nacht kann außerdem Aktualisierungen verlieren; beim Reconnect wird unabhängig vom Modus wieder abonniert. | `src/main.cpp`, `messageHandler()`, `connectedHandler()` |
| F04 | P1 | Zeilenzahl, LED-Anzahl und vier feste Kalenderinitialisierungen sind unabhängig. Eine kleinere Zeilenzahl erzeugt ungültige Arrayzugriffe; zusätzliche Zeilen bleiben falsch initialisiert. | `src/main.cpp`, Konstanten und `setup()` |
| F05 | P2 | `if (time_hours)` verhindert den Stundenwechsel von 0 auf 1. Der ungültige Initialwert −1 wird dagegen als wahr behandelt. | `src/main.cpp`, `increaseSecondsOfTime()` |
| F06 | P2 | Beim Zurücksetzen der Blinkmarkierung wird die Farbe der aktuellen Stunde an den zuvor markierten Index geschrieben. Stundenwechsel und Zeitsprünge können falsche Farben hinterlassen. | `src/main.cpp`, `blinkEverySecond()` |
| F07 | P2 | Vergangene Stunden werden nur bei veränderten Termindaten grau dargestellt. Der Zeitablauf allein löst die nötige Neuberechnung nicht aus. | `src/main.cpp`, `messageHandler()` |
| F08 | P2 | Die interne Uhr holt pro Schleifendurchlauf höchstens eine Sekunde nach. Laufzeit und blockierende Verzögerungen führen zu Drift. | `src/main.cpp`, `loop()` |
| F09 | P2 | Der Zeitstempel der letzten LED-Ausgabe wird nur im Verzögerungszweig aktualisiert. Die vorgesehene Begrenzung der Ausgabefrequenz funktioniert dadurch nicht zuverlässig. | `src/main.cpp`, `show_leds()` |
| F10 | P3 | Beim Erzeugen eines Termins wird der Literaltext `"summary"` statt der eingelesenen Variable verwendet. | `lib/Appointment/Calendar.cpp`, `parseAppointments()` |

## 3. Offene Regeln und Robustheitslücken

Diese Punkte sind vor oder während der jeweiligen Umsetzung zu entscheiden. Sie sind nicht alle bereits nachgewiesene Fehler im Zusammenspiel mit dem tatsächlichen Sender.

- **Zeitvertrag:** Welche Datums- und Zeitzonenangaben sendet der Produzent? Filtert er bereits auf den angezeigten Tag und lokalisiert die Uhrzeiten? Der aktuelle Parser liest nur Stunde und Minute aus festen Zeichenpositionen und verwirft Datum und Zeitzone.
- **Tagesgrenzen:** Wie werden ganztägige, mehrtägige und über Mitternacht laufende Termine behandelt? Die aktuelle Intervallprüfung deckt Zeiträume über Mitternacht nicht korrekt ab. Beim Tageswechsel muss außerdem feststehen, wann alte Daten ungültig werden.
- **Überlappende Termine:** Derzeit gewinnt der erste passende Termin in der Nachricht. Die gewünschte Prioritätsregel muss ausdrücklich festgelegt werden.
- **Übrige LED-Positionen:** Für die sieben Positionen außerhalb der Stundenanzeige ist ein definierter Zustand nötig. Derzeit blinkt Position 25 bei unbekannter Uhrzeit; ansonsten werden diese Positionen nur in der Startanimation genutzt.
- **Nachrichtenvalidierung:** Arraytyp, Pflichtfelder, Zeitwerte und Farbformat prüfen. Ungültige Nachrichten sollen den letzten gültigen Zustand erhalten und einen nachvollziehbaren Fehler liefern. Ein führendes `#` wird vom aktuellen Hex-Parser nicht als Bestandteil eines Farbwerts unterstützt.
- **Kapazität:** Maximale Terminanzahl und Nachrichtengröße festlegen und mit dem MQTT-Empfangspuffer abstimmen. Die Allokation pro Nachricht wird freigegeben, benötigt aber eine klare Obergrenze.
- **Zustellung und Wiederverbindung:** Snapshot- oder Änderungsnachrichten, Retain-Verhalten und Wiederherstellung nach Verbindungsverlust dokumentieren. Rückgabewerte der MQTT-Operationen werden derzeit nicht ausgewertet.
- **Start ohne WLAN:** Der aktuelle Start wartet auf WLAN beziehungsweise startet bei Fehlschlag neu. Das gewünschte Anzeigeverhalten bei fehlendem Netz ist festzulegen.

## 4. Zielstruktur nach Clean Code und DDD

Die Struktur soll für eine kleine Firmware angemessen bleiben. Maßgeblich sind klare Verantwortlichkeiten, verständliche Fachbegriffe und überprüfbare Zustände. Ein umfangreiches DDD-Framework ist nicht erforderlich.

| Verantwortung | Ziel |
| --- | --- |
| Kalenderidentität und Kalenderzustand | Kalender unabhängig von MQTT-Topics und physischen LED-Indizes beschreiben. Bewusst entscheiden, ob Termine oder eine für die Tagesansicht ausreichende Projektion gehalten werden. |
| Zeit- und Terminregeln | Gültige Zeiträume und die vereinbarten Tages- und Überlappungsregeln ohne Netzwerk- oder LED-Zugriff berechnen. |
| Board-Konfiguration | Kalender den Zeilen zuordnen; Zeilenzahl und LED-Anzahl aus einer gemeinsamen Konfiguration ableiten. |
| Tagesansicht / Renderer | Aus Kalenderzustand, Uhrzeit und Anzeigemodus einen vollständigen LED-Puffer berechnen. |
| Strip-Layout | Zeile und Position auf die physische LED-Kette abbilden und Grenzen absichern. |
| MQTT-Adapter | Topics zuordnen, Eingaben validieren und in interne Daten beziehungsweise Aktionen übersetzen. |
| LED-Ausgabe | Den fertigen Bildpuffer über FastLED ausgeben. |

Für gleichgerichtet angeschlossene Zeilen gilt zentral:

```text
stripIndex = rowIndex * 31 + positionInRow
ledCount   = rowCount * 31
```

Die konkrete Verdrahtung ist bei der Hardwareprüfung zu bestätigen. Kalenderobjekte sollen keine eigenen physischen Offsets benötigen.

Der heutige `BlinkyCalendar` vermischt MQTT-Topic, Strip-Offset und Stundenfarben. `messageHandler()` vermischt Routing, Parsing, Zustandsänderung und Ausgabe. Diese Grenzen werden schrittweise getrennt. Fachlogik soll ohne Arduino_JSON, MQTT und FastLED prüfbar werden.

## 5. Umsetzung in Etappen

### Etappe 1: Fehler mit hoher Priorität absichern

- [ ] F01: Unsichere Debug-Stringbildung korrigieren.
- [ ] F02/F03: Nachtmodus als Anzeigezustand behandeln; Termindaten weiter empfangen und beim Aufwachen vollständig neu zeichnen.
- [ ] F04: Eine gemeinsame Zeilenkonfiguration einführen. Zeilenzahl, LED-Anzahl, Initialisierung und Schleifen daraus ableiten.
- [ ] Zunächst unterschiedliche Zeilenzahlen zur Compile-Zeit unterstützen. Eine Laufzeitkonfiguration ist vorerst nicht erforderlich.

**Abnahmekriterien:** Nachtmodus bleibt über mehrere Blinkzyklen und einen MQTT-Reconnect dunkel. Nach dem Aufwachen sind auch unveränderte Termine wieder sichtbar. Konfigurationen mit 1, 4 und 6 Zeilen verwenden ausschließlich gültige, getrennte LED-Bereiche.

### Etappe 2: Zeit und Darstellung konsistent machen

- [ ] F05/F08: Gültigkeit der Uhrzeit explizit behandeln und Fortschreibung aus tatsächlich verstrichener Zeit ableiten.
- [ ] Einen gemeinsamen Renderer für die Tagesansicht einführen.
- [ ] F06/F07: Blinkmarkierung und Darstellung vergangener Stunden aus dem aktuellen Zustand berechnen.
- [ ] Den Zustand der sieben übrigen LED-Positionen festlegen und vollständig rendern.
- [ ] F09: Ausgabezeitpunkt korrekt verwalten; blockierende Wartezeiten im normalen Betrieb beseitigen. Die dokumentierte Empfindlichkeit der LED-Ansteuerung dabei auf Hardware prüfen.
- [ ] F10: Den eingelesenen Termintitel korrekt übernehmen.

**Abnahmekriterien:** 00:59:59 wechselt korrekt auf 01:00:00. Auch 23:59:59, externe Zeitsprünge und unbekannte Uhrzeit sind abgedeckt. Stundenwechsel hinterlassen keine Blinkreste. Vergangene Stunden aktualisieren sich ohne neue Termine. Gleicher Zustand erzeugt denselben vollständigen LED-Puffer.

### Etappe 3: MQTT-Vertrag und fachliche Regeln festigen

- [ ] Zeit-, Tageswechsel- und Überlappungsregeln aus Abschnitt 3 entscheiden und dokumentieren.
- [ ] Beispielnachrichten mit Pflichtfeldern, Farbformat und Größenlimits dokumentieren.
- [ ] JSON-Parsing und Topic-Zuordnung von der Fachlogik trennen.
- [ ] Nachrichten vollständig validieren, bevor gültiger Kalenderzustand ersetzt wird.
- [ ] Verhalten bei leeren Terminlisten, ungültigen Nachrichten und Verbindungsverlust implementieren und prüfen.

**Abnahmekriterien:** Eine gültige leere Liste löscht Termine. Fehlerhafte Daten beschädigen keinen gültigen Zustand. Reihenfolge, Datumsbezug und Grenzen von Terminen verhalten sich gemäß dem dokumentierten Vertrag. Nach Wiederverbindung ist der Zustand reproduzierbar wiederherstellbar.

### Etappe 4: Struktur und Wartbarkeit abschließen

- [ ] Verbleibende Hardware- und Transportabhängigkeiten aus der Fachlogik entfernen.
- [ ] Öffentlichen veränderbaren Zustand reduzieren; Index- und Zeitinvarianten absichern.
- [ ] Aussagekräftige Namen und Konstanten vereinheitlichen, Include-Guards ergänzen und überholten Code entfernen.
- [ ] README um vollständige Einrichtung und Konfiguration ergänzen. Insbesondere `CALENDAR_BOARD_NAME`, `MQTT_HOST` und die zusätzliche Upload-Konfiguration erläutern.
- [ ] Reproduzierbaren Build und die relevanten automatisierten Tests dokumentieren.

**Abnahmekriterien:** Änderungen an MQTT oder FastLED erfordern keine Änderung der fachlichen Intervallregeln. Ein weiteres Board lässt sich über seine Zeilenkonfiguration einrichten. Einrichtung, Build und Prüfungen sind nachvollziehbar beschrieben.

## 6. Prüfstrategie

Automatisierte Tests sollen Verhalten absichern und schrittweise mit den Änderungen entstehen:

- Zeitintervalle: genaue Stundenbegrenzungen, unmittelbar angrenzende und überlappende Termine; Tagesgrenzen gemäß Vertrag.
- Strip-Layout: erste und letzte Position jeder Zeile sowie unzulässige Indizes bei mehreren Zeilenzahlen.
- Rendering: Zeitwechsel, Blinkphasen, Nachtmodus und Wiederaufnahme mit unveränderten Daten.
- MQTT-Eingaben: gültige, leere, fehlerhafte und zu große Nachrichten.

Auf echter Hardware werden LED-Zuordnung, Farben, Flackern, Nachtmodus, MQTT-Reconnect und OTA während des Betriebs geprüft. Ein Host-Test ersetzt diese Prüfungen nicht.

## 7. Fortschritt pflegen

Nach jeder Etappe die erledigten Aufgaben abhaken und hier das Ergebnis festhalten. Ein Fehler gilt erst mit passender Prüfung als erledigt. Offene Entscheidungen werden in Abschnitt 3 aufgelöst und in die fachlichen Leitplanken beziehungsweise den Nachrichtenvertrag übernommen.

| Datum | Änderung / erledigte IDs | Prüfung und Ergebnis | Noch offen |
| --- | --- | --- | --- |
| 2026-09-11 | Bestandsaufnahme und Plan angelegt | Statische Quellcodeanalyse; kein Build oder Hardwaretest | Umsetzung aller Etappen |


## Fortschritt der Senderimplementierung

VirtualEntities enthält inzwischen einen optionalen v1-Publisher für Tagesdaten und Datum/Uhrzeit. Die erste Tagesprojektion, die Quellenaktualisierung und der Publisher wurden separat committet; 204 Unit-Tests bestehen. Konfiguration und Protokoll sind dort in `docs/calendarboard.md` beschrieben. Details und Grenzen stehen im [VirtualEntities-Plan](virtualentities-weiterentwicklungsplan.md).

Nächster Board-Schritt: v1-Nachrichten validieren, Zeilen über stabile IDs zuordnen, Datum/Uhrzeit atomar übernehmen und Synchronisationsanfragen bei Start, Wiederverbindung und Tageswechsel senden. Erst anschließend wird die neue Versorgung aktiviert. Die bekannten Firmwarefehler aus Abschnitt 2 sind durch die Senderänderungen noch nicht behoben.


## Umsetzung der Firmware auf v1

- `64f8579`: Hardwareunabhängiger Boardzustand, Protokollvalidierung, lokale Uhr und vollständiges Rendering. Gemeinsame JSON-Fixtures aus VirtualEntities.
- Firmware-Integration: Zeilenkonfiguration in `include/BoardConfig.h`, MQTT-v1-Topics und Synchronisation bei Start, Reconnect und Tageswechsel. Der alte Terminparser und `BlinkyCalendar` sind entfernt.

F01 und F10 entfallen mit dem alten Parser/Handler. F02/F03 und F06/F07 werden durch vollständiges zustandsbasiertes Rendering behoben. F04 wird durch die gemeinsame Zeilenkonfiguration behoben; F05/F08 durch die UTC-basierte Fortschreibung mit Restmillisekunden und lokaler Zeitzonenregel; F09 durch nicht blockierende Ausgabetaktung. Alle Befunde sind damit auf Codeebene bearbeitet. Die Hardware-Abnahmekriterien sind noch offen und gelten nicht als abgeschlossen.

Zusätzlicher Befund behoben: Der MQTT-Client benötigt einen dauerhaft gültigen Hostnamen-Zeiger. Der Helper hält diesen jetzt selbst vor. Fehler bei Subscriptions führen zu einer erneuten Verbindung; ausgehende Synchronisationsanfragen werden außerhalb des Nachrichten-Callbacks verarbeitet.

Prüfung: Native C++-Tests mit AddressSanitizer/UndefinedBehaviorSanitizer bestanden; reale Sender-Fixtures werden dekodiert. Firmware-Build für D1 mini mit espressif8266 2.6.3 erfolgreich: 31.012 Bytes RAM (37,9 %), 341.000 Bytes Flash (32,6 %). Keine produktive Aktivierung, kein Upload und noch keine echte Broker-/Hardwareabnahme.

Vor dem Hardwaretest: VirtualEntities-Boardkonfiguration und Zeilen-IDs abgleichen. Diese Firmware erwartet v1; für einen Rückweg die vorherige Firmware bereithalten. Uhr und Kalenderstand sind nur im RAM. Konfigurierbare IANA-/POSIX-Zeitzonen müssen zueinander passen. Details und Befehle stehen in der README.

## Integrationstest und Installationsabgleich

Der gemeinsame Hosttest mit echtem lokalem MQTT-Broker besteht nun auch für
Broker-Neustart und automatische Wiederverbindung. Produktiver Vorabtest:
Board online, aber bislang keine v1-Antwort des Publishers. Der Firmware-Upload
wartet deshalb auf den Abgleich der vom Server-Service verwendeten Konfiguration.
Einzelheiten und offene Hardwarekriterien stehen in
[Integration und Hardwareabnahme](integration-abnahme.md).

## Hardwarefortschritt

Die v1-Firmware ist per OTA installiert. Das Board meldet sich danach wieder
online; der Publisher liefert aktuelle Zeit und vier Tageszeilen. Sichtprüfung:
Zeilenzuordnung, Stundenmarkierung und Weiß/Rot korrekt, kein störendes
Flackern. Nachtmodus wurde sichtbar geprüft. Ein kontrollierter MQTT-Reconnect
lieferte erneut Zeit und alle vier Zeilen. Offen bleiben ein leerer Tagesstand
und der Tageswechsel auf echter Hardware. Der separate lokale Brokertest deckt
diese Fälle bereits automatisiert ab, ersetzt die Hardwareprüfung aber nicht.

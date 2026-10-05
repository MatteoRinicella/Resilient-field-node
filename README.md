# Resilient Field Node

<p align="center">
  <img src="docs/images/project-banner.svg" alt="Resilient Field Node" width="100%">
</p>

<p align="center">
  Nodo di campo basato su ESP32 con misura reale della corrente, protezione locale fail-safe, relè, OLED, RTC e connettività resiliente.
</p>

## Prototipo

<table>
  <tr>
    <td align="center" colspan="2"><img src="docs/images/prototype-overview.jpg" alt="Vista completa del prototipo"><br><strong>Prototipo completo</strong></td>
  </tr>
  <tr>
    <td align="center"><img src="docs/images/display-normal.jpg" alt="OLED nello stato normale"><br><strong>Funzionamento normale</strong></td>
    <td align="center"><img src="docs/images/display-trip.jpg" alt="OLED dopo il trip"><br><strong>Protezione intervenuta</strong></td>
  </tr>
</table>

## Video dimostrativo

<p align="center">
  <a href="https://github.com/MatteoRinicella/Resilient-field-node/issues/1">
    <img src="docs/images/video-cover.png" alt="Guarda il video dimostrativo della protezione" width="760">
  </a>
</p>

## Obiettivo della prima milestone

Realizzare un MVP software eseguibile su PC che simuli un nodo di campo capace di acquisire misure, applicare una protezione locale e conservare gli eventi anche quando il gateway non è disponibile.

Quando la comunicazione ritorna, il nodo deve sincronizzare gli eventi senza perderli o registrarli due volte. Lo stato fisico più recente del nodo deve prevalere sulle informazioni obsolete conservate dal gateway.

## Primo scenario verificabile

1. Il nodo e il gateway funzionano normalmente.
2. La comunicazione con il gateway viene interrotta.
3. Viene simulata una sovracorrente.
4. Il nodo apre localmente l'interruttore simulato.
5. L'evento di trip viene conservato sul nodo.
6. La comunicazione viene ripristinata.
7. Il gateway riceve l'evento una sola volta e aggiorna lo stato corrente.

## Prima decisione progettuale

La protezione contro la sovracorrente viene eseguita dal nodo di campo.

Questa funzione non dipende dal gateway perché, durante una perdita della comunicazione, il nodo deve poter rilevare il guasto e aprire localmente l'interruttore simulato. Il gateway riceverà e conserverà l'evento quando la comunicazione sarà disponibile.

## Seconda decisione progettuale

Il nodo salva ogni evento in una memoria persistente locale prima di tentare la trasmissione.

L'evento rimane nella coda di trasmissione con stato `PENDING` finché il gateway non conferma di averlo registrato mediante un acknowledgment (`ACK`). Dopo la conferma, l'evento viene marcato come `ACKNOWLEDGED` e può essere eliminato in futuro secondo una politica di conservazione.

Gli eventi ancora `PENDING` non devono mai essere eliminati o sovrascritti per liberare spazio.

## Terza decisione progettuale

Ogni evento riceve un `event_id` univoco nel momento in cui viene creato. Gli eventuali tentativi di ritrasmissione mantengono lo stesso `event_id`.

Il gateway usa `event_id` per riconoscere gli eventi già registrati: un duplicato non produce un secondo record, ma il gateway restituisce comunque l'ACK affinché il nodo possa completare la sincronizzazione.

## Quarta decisione progettuale

A ogni avvio il nodo genera un nuovo `boot_id` univoco. Il campo `sequence` è un contatore progressivo che ordina gli eventi prodotti durante quello specifico avvio e può ripartire da 1 dopo un riavvio.

La combinazione `device_id`, `boot_id` e `sequence` permette di distinguere e ordinare gli eventi anche quando il contatore riparte. `event_id` continua invece a essere la chiave usata per la deduplicazione.

## Quinta decisione progettuale

Ogni evento distingue tre momenti:

- `occurred_at`: quando il fatto è avvenuto sul nodo;
- `recorded_at`: quando il nodo ha registrato localmente l'evento;
- `received_at`: quando il gateway ha ricevuto l'evento.

Una trasmissione ritardata non modifica `occurred_at`. I timestamp vengono rappresentati in UTC usando il formato ISO 8601.

## Sesta decisione progettuale

Alla riconnessione il nodo invia prima uno snapshot autorevole del proprio stato fisico attuale e soltanto dopo trasmette gli eventi arretrati.

Lo snapshot risponde alla domanda "qual è lo stato adesso?", mentre lo storico risponde alla domanda "che cosa è accaduto?". Gli eventi storici non devono sovrascrivere uno stato corrente più recente.

## Settima decisione progettuale

Durante una disconnessione continuano a funzionare sul nodo:

- acquisizione dei sensori;
- protezione locale;
- comando dell'interruttore simulato;
- persistenza locale degli eventi.

Il gateway perde temporaneamente la visibilità aggiornata e il telecontrollo, ma non appartiene al percorso critico della protezione.

## Ottava decisione progettuale

La connettività e la sincronizzazione del nodo sono descritte inizialmente da tre stati:

- `NORMAL`: gateway raggiungibile e sincronizzazione completata;
- `OFFLINE`: gateway non raggiungibile, con funzioni locali ancora operative;
- `SYNCING`: collegamento ripristinato, ma snapshot ed eventi arretrati non ancora completamente riconciliati.

Le transizioni principali sono `NORMAL -> OFFLINE` alla perdita della comunicazione, `OFFLINE -> SYNCING` al ripristino e `SYNCING -> NORMAL` soltanto dopo la conferma di snapshot ed eventi. Una nuova perdita di comunicazione provoca `SYNCING -> OFFLINE`.

## Nona decisione progettuale

Subito dopo ogni avvio il nodo entra in `STARTING`. In questa fase recupera stato operativo ed eventi persistenti, genera un nuovo `boot_id`, inizializza `sequence`, registra l'avvio e verifica il collegamento con il gateway.

Un riavvio non equivale a un reset della protezione: se il nodo recupera uno stato `TRIPPED`, oppure non riesce a determinare in modo affidabile lo stato precedente, l'interruttore simulato rimane `OPEN`. Da `STARTING` il nodo passa a `OFFLINE` se il gateway non è raggiungibile, altrimenti entra in `SYNCING` prima di poter diventare `NORMAL`.

## Decima decisione progettuale

Il nodo rappresenta separatamente tre dimensioni:

- stato operativo e di comunicazione: `STARTING`, `NORMAL`, `OFFLINE`, `SYNCING`;
- stato della protezione: `ARMED`, `TRIPPED`;
- stato fisico dell'interruttore: `OPEN`, `CLOSED`.

Questa separazione permette, per esempio, di rappresentare contemporaneamente `OFFLINE + TRIPPED + OPEN`. La combinazione `TRIPPED + CLOSED` non è consentita.

## Undicesima decisione progettuale

Ogni misura contiene un valore e una qualità:

- `VALID`: misura recente, disponibile e nei limiti previsti;
- `SUSPECT`: misura disponibile ma non pienamente affidabile;
- `INVALID`: misura assente, impossibile o non utilizzabile;
- `STALE`: ultima misura disponibile non aggiornata entro il timeout.

Con il gateway raggiungibile, una misura necessaria non `VALID` porta il nodo in `DEGRADED`. Se la comunicazione è assente, lo stato operativo rimane `OFFLINE`, mentre la qualità resta associata alla singola misura.

## Dodicesima decisione progettuale

Se la misura di corrente è `INVALID` o `STALE`, il nodo entra in funzionamento degradato ma non apre automaticamente un interruttore già `CLOSED`. Mantiene la posizione esistente, registra un evento di anomalia del sensore e non usa il valore inaffidabile per valutare una sovracorrente.

Finché la misura di corrente non torna `VALID`, un comando `CLOSE_BREAKER` viene rifiutato. Questa è una politica esplicita del proof of concept e non rappresenta una regola universale per sistemi di protezione reali.

## Funzionalità implementate nella versione Python

La versione Python del proof of concept implementa attualmente:

- misure di corrente con tempo, valore e qualità;
- qualità `VALID`, `SUSPECT`, `INVALID` e `STALE`;
- protezione di sovracorrente con soglia e ritardo configurabili;
- memoria persistente del trip durante l'esecuzione;
- apertura locale dell'interruttore simulato;
- reset controllato della protezione;
- blocco della richiusura con trip attivo o misura non valida;
- generazione di eventi con `event_id`, `boot_id` e `sequence`;
- salvataggio persistente degli eventi in SQLite;
- stati di consegna `PENDING` e `ACKNOWLEDGED`;
- gateway simulato con deduplicazione tramite `event_id`;
- gateway HTTP Flask pubblicato su PythonAnywhere e raggiungibile tramite HTTPS, con endpoint diagnostico [`GET /health`](https://mrculino.eu.pythonanywhere.com/health) verificato localmente e dall'esterno;
- archivio SQLite del gateway predisposto per salvare gli eventi ricevuti e ignorare in modo idempotente le ritrasmissioni con lo stesso `event_id`;
- archivio SQLite separato per lo snapshot dello stato corrente, con una sola riga per dispositivo e aggiornamento atomico tramite `UPSERT`;
- metodi di lettura SQLite per recuperare lo stato più recente e un numero configurabile di eventi recenti, usati dal visualizzatore da terminale;
- endpoint HTTP `POST /state` verificato localmente con autenticazione Bearer, validazione dello snapshot e ACK restituito soltanto dopo il salvataggio SQLite;
- endpoint HTTP `POST /events` pubblicato su PythonAnywhere e verificato online con autenticazione Bearer fail-closed, validazione del JSON, salvataggio SQLite prima dell'ACK e riconoscimento idempotente dei duplicati;
- client HTTPS ESP32 verificato sull'hardware con autenticazione Bearer, validazione del certificato del server, invio FIFO degli eventi `PENDING` e marcatura `ACKNOWLEDGED` soltanto dopo l'ACK del gateway online;
- snapshot autorevole ESP32 inviato a `POST /state` prima degli eventi arretrati e ritrasmesso soltanto quando cambiano trip, interruttore o ultima sequenza;
- verifica dello stato dopo ogni ACK dello snapshot, così un cambiamento avvenuto durante la richiesta HTTPS invalida l'ACK ormai superato e forza una nuova sincronizzazione prima dell'invio degli eventi;
- sincronizzazione degli eventi dopo una disconnessione;
- stati operativi `STARTING`, `NORMAL`, `OFFLINE` e `SYNCING`;
- configurazione della protezione tramite file TOML;
- test unitari e di integrazione eseguibili insieme.

## Funzionalità implementate nella versione C

La versione C riproduce la logica locale destinata a essere portata in futuro su un dispositivo embedded:

- misura di corrente rappresentata da una `struct` con tempo, valore e qualità;
- qualità `VALID`, `SUSPECT`, `INVALID` e `STALE` rappresentate tramite `enum`;
- rilevazione della sovracorrente con soglia configurabile;
- ritardo temporale prima del trip;
- interruzione del conteggio in presenza di misure `INVALID` o `STALE`;
- memoria del trip fino a un reset valido;
- apertura dell'interruttore simulato in seguito al trip;
- reset consentito solo con misura `VALID` e corrente sotto soglia;
- richiusura negata con trip attivo o misura non `VALID`;
- separazione tra interfaccia pubblica `.h`, implementazione `.c` e simulazione;
- test automatici delle regole principali tramite `assert`.

La versione C non replica database, gateway e sincronizzazione: queste funzioni restano nella simulazione Python. Il codice C dimostra invece la logica locale essenziale che non deve dipendere dalla disponibilità della rete.

## Firmware ESP32 e prototipo hardware

La logica C è stata portata su un ESP32 classico usando ESP-IDF 6.1. Il firmware esegue la protezione locale su hardware reale e mantiene il percorso critico indipendente dalla rete.

Sono attualmente implementati e verificati:

- avvio fail-safe con relè inizialmente aperto;
- chiusura del relè soltanto dopo la verifica del sensore INA219;
- misura reale della corrente tramite INA219 all'indirizzo I2C `0x40`;
- soglia di sovracorrente impostata a `17 mA` per il carico dimostrativo a LED;
- ritardo di trip di `2 s`;
- apertura fisica del relè su `GPIO27` in seguito al trip;
- timestamp reale dell'evento `PROTECTION_TRIP`, acquisito dal DS3231 al momento dell'apertura;
- creazione di un evento strutturato `NodeEvent` con sequenza progressiva, timestamp, corrente misurata, soglia e stato dell'interruttore;
- coda persistente nella NVS interna fino a 16 `NodeEvent`, inseriti inizialmente nello stato `PENDING` senza sovrascrivere quelli già presenti;
- recupero dopo il riavvio dell'intera coda, dell'ultimo evento e della sequenza progressiva;
- selezione FIFO del `NodeEvent` `PENDING` più vecchio come prossimo evento da trasmettere;
- marcatura persistente per sequenza degli eventi consegnati come `ACKNOWLEDGED`, con gestione idempotente degli ACK duplicati;
- persistenza e ripristino dello stato della protezione (`tripped`) e della posizione del relè, comprese le condizioni `FALSE/OPEN` e `FALSE/CLOSED`;
- mantenimento dello stato di trip fino a un reset valido;
- reset e richiusura controllati tramite il pulsante BOOT su `GPIO0`;
- controllo alternativo tramite il monitor seriale USB con i comandi `reset`, `close` e `restart`, utile quando i pulsanti della scheda non sono fisicamente accessibili;
- display OLED 128x64 all'indirizzo I2C `0x3C` con corrente, soglia, stato della protezione e stato del carico;
- RTC DS3231 all'indirizzo I2C `0x68`, con lettura e scrittura in formato 24 ore e gestione automatica del fuso orario italiano;
- connessione Wi-Fi in un'attività FreeRTOS separata, senza interrompere il ciclo di protezione;
- configurazione locale di SSID e password tramite `Kconfig.projbuild`, con `sdkconfig` escluso dal repository;
- acquisizione automatica dell'indirizzo IP tramite DHCP.

La sincronizzazione SNTP/NTP è stata verificata end-to-end: dopo aver ottenuto la connessione Wi-Fi, l'ESP32 acquisisce il tempo di rete, applica automaticamente il fuso italiano `CET/CEST`, lo converte in `RtcDateTime` e aggiorna il DS3231 senza interrompere il ciclo di protezione. La futura serializzazione degli eventi continuerà a normalizzare i timestamp nel formato UTC ISO 8601 previsto dall'architettura.

### Collegamenti principali del prototipo

| Componente | Collegamento ESP32 | Funzione |
|---|---|---|
| Relè 5 V | `GPIO27` | Apertura e chiusura del carico |
| Pulsante BOOT | `GPIO0` | Reset del trip e richiusura controllata |
| Bus I2C SDA | `GPIO21` | Dati per INA219, OLED e DS3231 |
| Bus I2C SCL | `GPIO22` | Clock per INA219, OLED e DS3231 |
| INA219 | I2C `0x40` | Misura della corrente del carico |
| OLED 128x64 | I2C `0x3C` | Interfaccia locale del nodo |
| DS3231 | I2C `0x68` | Orologio hardware per i timestamp offline |

Il carico dimostrativo usa LED a bassa tensione e resistenze dedicate. Il prototipo non è progettato per essere collegato direttamente alla rete elettrica o a carichi industriali.

## Coerenza tra Python e C

Le due implementazioni usano le stesse regole fondamentali:

| Regola | Python | C |
|---|---|---|
| Sovracorrente | corrente `> soglia` | corrente `> soglia` |
| Ritardo del trip | durata `>= ritardo` | durata `>= ritardo` |
| Qualità utilizzabili dalla protezione | `VALID`, `SUSPECT` | `VALID`, `SUSPECT` |
| Reset | trip attivo, misura `VALID`, corrente sotto soglia | stesse condizioni |
| Richiusura | trip assente e misura `VALID` | stesse condizioni |

## Struttura principale

```text
config/
    node.toml

scripts/
    measurement_example.py
    run_all_tests.py

src/resilient_field_node/
    configuration.py
    gateway.py
    node.py
    synchronization.py

    domain/
        breaker.py
        event.py
        measurement.py
        measurement_quality.py
        operational_state.py
        protection.py

    storage/
        sqlite_event_store.py

tests/
    unit/
    integration/

c/
    include/
        resilient_field_node.h
    src/
        main.c
        resilient_field_node.c
    tests/
        test_resilient_field_node.c
    build/

firmware/esp32/
    CMakeLists.txt

    main/
        Kconfig.projbuild
        esp32.c
        oled_display.c
        oled_display.h
        rtc_ds3231.c
        rtc_ds3231.h
        wifi_connection.c
        wifi_connection.h
        network_time.c
        network_time.h
```

## Preparazione dell'ambiente

Dalla cartella principale del progetto:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --editable .
```

## Esecuzione dei test Python

```powershell
python .\scripts\run_all_tests.py
```

Risultato atteso:

```text
Test superati: 16 / 16
Tutti i test sono stati superati
```

## Esecuzione dei test C

Con GCC disponibile, dalla cartella principale del progetto:

```powershell
gcc -I ".\c\include" ".\c\tests\test_resilient_field_node.c" ".\c\src\resilient_field_node.c" -Wall -Wextra -std=c11 -o ".\c\build\test_resilient_field_node.exe"
.\c\build\test_resilient_field_node.exe
```

Risultato atteso:

```text
Test soglia: OK
Test ritardo trip: OK
Test qualita misura: OK
Test reset protezione: OK
Test richiusura interruttore: OK
Tutti i test C sono stati superati
```

In VS Code è possibile ottenere lo stesso risultato aprendo `c/tests/test_resilient_field_node.c` e premendo il tasto Play.

## Esecuzione della simulazione Python

```powershell
python .\scripts\measurement_example.py
```

La simulazione dimostra:

1. avvio del nodo con gateway offline;
2. acquisizione di misure valide;
3. rilevazione di una sovracorrente persistente;
4. apertura locale dell'interruttore;
5. salvataggio persistente dell'evento;
6. mancata consegna durante la disconnessione;
7. passaggio a `SYNCING` alla riconnessione;
8. consegna dell'evento e ricezione dell'ACK;
9. ritorno allo stato `NORMAL`;
10. reset manuale e richiusura controllata.

## Esecuzione della simulazione C

Da terminale:

```powershell
gcc -I ".\c\include" ".\c\src\main.c" ".\c\src\resilient_field_node.c" -Wall -Wextra -std=c11 -o ".\c\build\main.exe"
.\c\build\main.exe
```

In VS Code è sufficiente aprire `c/src/main.c` e premere il tasto Play. La simulazione mostra misure con qualità differenti, sovracorrente temporizzata, trip, apertura dell'interruttore, tentativi di reset non sicuri e richiusura controllata.

## Visualizzazione dei dati del gateway da terminale

Lo stato corrente e gli ultimi dieci eventi ricevuti dal gateway possono essere mostrati senza interrogare manualmente SQLite:

```bash
python scripts/show_gateway_data.py
```

Per aggiornare automaticamente la visualizzazione ogni cinque secondi durante una dimostrazione:

```bash
python scripts/show_gateway_data.py --watch
```

La modalità continua si interrompe con `Ctrl+C`. Il visualizzatore mostra identificativo del dispositivo, stato della protezione, posizione dell'interruttore, ultima sequenza e storico dei trip con corrente, soglia e orario. I timestamp restano completi nel database ma vengono presentati senza il suffisso del fuso orario.

## Compilazione e caricamento del firmware ESP32

Il progetto ESP-IDF si trova in `firmware/esp32` ed è configurato per il target `esp32`.

Da VS Code, con l'estensione ESP-IDF configurata:

1. aprire `firmware/esp32` come directory del progetto ESP-IDF;
2. usare **Build Project** per compilare;
3. usare **Build, Flash and Monitor** per compilare, caricare il firmware e aprire il monitor seriale;
4. usare `COM4` quando la scheda viene rilevata come convertitore CP210x.

Le credenziali Wi-Fi, l'URL del gateway e il token API si configurano tramite **ESP-IDF: SDK Configuration Editor (menuconfig)** nella sezione `Resilient Field Node`. Non devono essere scritti nel codice né inseriti nel README.

### Comandi dal monitor seriale USB

Con il monitor seriale ESP-IDF aperto è possibile controllare il nodo senza accedere fisicamente ai pulsanti della scheda. I caratteri digitati potrebbero non essere mostrati dal terminale; il comando viene comunque inviato premendo **Invio**.

- `reset`: azzera un trip valido, ma mantiene il relè aperto;
- `close`: richiude il relè soltanto quando il trip è disattivato e non è presente una sovracorrente;
- `restart`: riavvia il firmware via software senza cancellare la memoria NVS.

La sequenza normale dopo un intervento è quindi: rimuovere la causa della sovracorrente, inviare `reset` e poi inviare `close`. Il pulsante BOOT continua a essere supportato come comando locale alternativo.

Durante le prove è stato verificato che l'ESP32 continua ad acquisire la corrente e applicare la protezione mentre il Wi-Fi completa in parallelo l'associazione, ottiene un indirizzo IP e sincronizza il DS3231 tramite NTP con l'ora locale italiana.

È stato inoltre verificato sull'hardware che il superamento continuativo della soglia genera un `NodeEvent`, apre il relè e produce un record diagnostico completo, ad esempio `EVENTO #1: PROTECTION_TRIP` con data, ora, corrente, soglia e stato `OPEN`. Il successivo reset del trip e la richiusura manuale restano due operazioni distinte e controllate.

La coda persistente è stata verificata generando più trip consecutivi: gli eventi sono stati conservati come `PENDING`, il conteggio è aumentato senza perdita o sovrascrittura e, dopo il riavvio dell'ESP32, l'intera coda è stata recuperata. Con gli eventi `#4`, `#5` e `#6` presenti, il firmware ha continuato a indicare correttamente `#4` come prossimo evento da inviare, verificando l'ordinamento FIFO. Anche lo stato `Trip: TRUE` e l'interruttore `OPEN` sono stati ripristinati correttamente.

La gestione dell'acknowledgment è stata verificata marcando l'evento `#4` come `ACKNOWLEDGED`: il conteggio `PENDING` è sceso da tre a due e il prossimo evento selezionato è diventato `#5`. Un secondo ACK per `#4` non ha modificato nuovamente la coda, confermando il comportamento idempotente.

Il collegamento reale ESP32-gateway è stato verificato inviando in ordine FIFO gli eventi persistenti `#5` e `#6` al servizio PythonAnywhere tramite HTTPS. Per ciascun evento il firmware ha validato il certificato del server, ha ricevuto l'ACK applicativo e ha quindi aggiornato la voce NVS da `PENDING` ad `ACKNOWLEDGED`. Il secondo evento è stato trasmesso soltanto dopo il completamento del primo, mentre il ciclo locale di misura e protezione ha continuato a funzionare in parallelo.

Dopo un riavvio hardware la coda è stata recuperata con tre eventi totali e zero eventi `PENDING`; il firmware ha mantenuto la prossima sequenza a `#7` e non ha ritrasmesso `#5` o `#6`, confermando la persistenza degli ACK nella NVS.

La persistenza lato gateway è stata controllata direttamente nel database SQLite online: gli eventi `esp32-80f3da543d4c-event-5` e `esp32-80f3da543d4c-event-6` risultano registrati una sola volta con sequenze `5` e `6`, tipo `PROTECTION_TRIP` e interruttore `OPEN`.

È stato inoltre verificato localmente l'archivio dello stato corrente: due snapshot consecutivi dello stesso `device_id` producono una sola riga e il secondo aggiorna correttamente trip, posizione dell'interruttore, sequenza e orari senza modificare lo storico degli eventi.

Il nuovo endpoint `POST /state` è stato verificato attraverso il client di test Flask: un token errato restituisce `401`, due snapshot validi ricevono l'ACK e il secondo aggiorna la riga SQLite del dispositivo da `TRUE/OPEN` a `FALSE/CLOSED`.

La stessa operazione è stata verificata sul gateway pubblico PythonAnywhere: due richieste HTTPS per `manual-state-test` hanno prodotto una sola riga, aggiornata al secondo snapshot con `boot_id` `test-boot-002`, sequenza `7`, trip attivo e interruttore `OPEN`.

L'invio reale dello snapshot è stato verificato sull'ESP32: dopo Wi-Fi e sincronizzazione NTP il nodo ha trasmesso la sequenza `#6` con trip `FALSE` e interruttore `CLOSED`, ha validato il certificato PythonAnywhere e ha ricevuto `ACKNOWLEDGED`. Nei cicli successivi lo snapshot non è stato ritrasmesso perché lo stato non era cambiato, mentre l'acquisizione della corrente ha continuato a funzionare durante la richiesta HTTPS.

La persistenza dello stesso snapshot è stata verificata direttamente nel database online: il record del dispositivo `esp32-80f3da543d4c` contiene il `boot_id` reale dell'avvio, timestamp italiano con offset `+02:00`, ultima sequenza `6`, trip `0` e interruttore `CLOSED`.

La protezione contro gli ACK superati è stata verificata sull'hardware durante cambiamenti reali dello stato. Negli eventi `#9` e `#10` il trip è intervenuto mentre era ancora in corso l'invio dello snapshot precedente `FALSE/CLOSED`: il firmware ha rilevato la differenza dopo l'ACK, ha stampato `stato cambiato durante l'invio`, ha trasmesso prima il nuovo snapshot `TRUE/OPEN` con la sequenza aggiornata e soltanto dopo il relativo ACK ha inviato l'evento `PROTECTION_TRIP`. Anche un reset del trip e una richiusura avvenuti durante una richiesta sono stati rilevati e risincronizzati. Un riavvio intermedio ha inoltre recuperato dalla NVS lo stato persistente `FALSE/OPEN`, l'ultima sequenza `#8` e una coda senza eventi pendenti. Durante le prove successive, fino all'evento `#11`, il ciclo locale di misura è rimasto operativo e il precedente timeout I2C non si è ripresentato.

La verifica conclusiva del database PythonAnywhere ha confermato che gli eventi `#9`, `#10` e `#11` sono presenti una sola volta, tutti come `PROTECTION_TRIP` con interruttore `OPEN` e con i rispettivi timestamp italiani. Lo snapshot corrente del dispositivo riporta correttamente l'ultima sequenza `#11`, trip disattivato e interruttore `CLOSED`, coerentemente con il reset e la richiusura eseguiti al termine della prova.

I comandi seriali USB sono stati verificati sull'hardware con la scheda racchiusa nel contenitore: `reset` ha disattivato il trip mantenendo il relè aperto, `close` ha richiuso il relè dopo la rimozione della condizione di guasto e `restart` ha riavviato correttamente il firmware. Il ciclo di protezione e il bus I2C sono rimasti operativi durante la prova.

La base dati del visualizzatore da terminale è stata verificata con test dedicati: lo store dello stato restituisce lo snapshot più recente, mentre lo store degli eventi restituisce lo storico in ordine dal più recente e applica correttamente il limite richiesto. Il comando normale e la modalità continua `--watch` sono stati provati localmente; la suite completa comprende 18 test superati.

## Limiti attuali del proof of concept

Non sono ancora implementati o completamente verificati:

- evento diagnostico per misura `INVALID` o `STALE`;
- stato operativo `DEGRADED`;
- politica di conservazione degli eventi già riconosciuti;
- integrazione del modulo Ethernet W5500;
- calibrazione metrologica dell'INA219;
- test di durata, gestione completa dei guasti e validazione real-time;
- hardware certificato per tensioni o correnti industriali.


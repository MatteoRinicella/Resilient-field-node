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
  <a href="docs/demo/resilient-field-node-demo.mp4?raw=1">
    <img src="docs/images/video-cover.svg" alt="Guarda il video dimostrativo" width="760">
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
- mantenimento dello stato di trip fino a un reset valido;
- reset e richiusura controllati tramite il pulsante BOOT su `GPIO0`;
- display OLED 128x64 all'indirizzo I2C `0x3C` con corrente, soglia, stato della protezione e stato del carico;
- RTC DS3231 all'indirizzo I2C `0x68`, con lettura e scrittura in formato 24 ore;
- connessione Wi-Fi in un'attività FreeRTOS separata, senza interrompere il ciclo di protezione;
- configurazione locale di SSID e password tramite `Kconfig.projbuild`, con `sdkconfig` escluso dal repository;
- acquisizione automatica dell'indirizzo IP tramite DHCP.

È in corso l'integrazione della sincronizzazione SNTP/NTP: il modulo di acquisizione dell'ora UTC e conversione in `RtcDateTime` è stato creato e compilato correttamente. Rimane da verificare end-to-end la sequenza rete -> ESP32 -> DS3231.

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

## Compilazione e caricamento del firmware ESP32

Il progetto ESP-IDF si trova in `firmware/esp32` ed è configurato per il target `esp32`.

Da VS Code, con l'estensione ESP-IDF configurata:

1. aprire `firmware/esp32` come directory del progetto ESP-IDF;
2. usare **Build Project** per compilare;
3. usare **Build, Flash and Monitor** per compilare, caricare il firmware e aprire il monitor seriale;
4. usare `COM4` quando la scheda viene rilevata come convertitore CP210x.

Le credenziali Wi-Fi si configurano tramite **ESP-IDF: SDK Configuration Editor (menuconfig)** nella sezione `Resilient Field Node`. Non devono essere scritte nel codice né inserite nel README.

Durante le prove è stato verificato che l'ESP32 continua ad acquisire la corrente e applicare la protezione mentre il Wi-Fi completa in parallelo l'associazione e ottiene un indirizzo IP.

## Limiti attuali del proof of concept

Non sono ancora implementati o completamente verificati:

- sincronizzazione NTP completa con aggiornamento automatico del DS3231;
- protocollo applicativo reale per comunicare con il gateway;
- invio dello snapshot autorevole dello stato;
- recupero persistente dello stato `TRIPPED` dopo un riavvio;
- persistenza degli eventi nella memoria dell'ESP32;
- evento diagnostico per misura `INVALID` o `STALE`;
- stato operativo `DEGRADED`;
- politica di conservazione degli eventi già riconosciuti;
- integrazione del modulo Ethernet W5500;
- calibrazione metrologica dell'INA219;
- test di durata, gestione completa dei guasti e validazione real-time;
- hardware certificato per tensioni o correnti industriali.

Questi elementi rappresentano evoluzioni successive e non devono essere confusi con le funzionalità già dimostrate dalle versioni Python e C del proof of concept.

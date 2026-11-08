# FlexRC

Kontroler RC (nadajnik + odbiornik) na ESP32-S3. Autor: Luke / Flex Electronics.
Repo: https://github.com/LukeSky91/FlexRC

**Stan:** schemat rev V1 gotowy, PCB zaprojektowane, **NIE zamówione**.

Firmware nadajnika jest **modularny i rozbudowany**, i **działa na płytce stykowej**
— to zweryfikowany, żywy punkt odniesienia sprzed przejścia na PCB. Ma pełne UI
OLED ze stronami ustawień, maszynę stanów łącza radiowego z armingiem/failsafe
i telemetrię zwrotną (bateria odbiornika) przez ACK payload nRF24.
Odbiornik (`rx_test_platform`) jest celowo prosty: PWM na 2 kanały, telemetria
baterii, **bez failsafe przy utracie linku**.
Obudowa jeszcze nieprojektowana.

---

## Narzędzia

| Obszar | Narzędzie |
|---|---|
| Firmware | VSCode |
| Schemat / PCB | KiCad 9.0.6 |
| Obudowa | Fusion 360 (druk 3D, ew. mosiężny panel przedni) |

---

## Sprzęt

**MCU:** Waveshare ESP32-S3-Pico (23803), układ ESP32-S3R2
— 16 MB flash (W25Q128), 2 MB PSRAM **quad** (GPIO35/36/37 wolne — wersje
  oktalne by je zabrały)
— wewnętrzny regulator: **MP28164 buck-boost**, 2 A, wejście = pin VSYS (39)
— zakres wejścia VSYS: **2,5–5,5 V**
— **D1 = MBR230LSFT1G**, Schottky VBUS → VSYS (topologia Pico)
— przyciski na module: **Key1 = GPIO0/BOOT**, **Key2 = CHIP_PU/RESET**
— **GPIO0 NIE jest wyprowadzony na listwę 40-pin** (tylko Key1 + testpoint P3).
  RUN jest dostępny na pinie 30.

**Zajęte przez moduł — NIE UŻYWAĆ:**
- `GPIO3` → USB_ADC (dzielnik R2/R4 100K od VBUS)
- `GPIO21` → wbudowana dioda WS2812B modułu
- `GPIO19 / GPIO20` → USB D− / D+
- `GPIO0` → przycisk BOOT, niedostępny na listwie

**Radio:** nRF24L01+ **PA/LNA** (antena zewnętrzna SMA — obudowa metalowa jest OK)
**Wyświetlacz:** SSD1306 128×64, I2C
**Wejścia:** 2 gimbale hallotronowe (z przyciskami), 7 przycisków (L/R/U/D/C + F1/F2)
**LED:** 5 × WS2812B na +5V, przez 74AHCT1G125 (level shifter 3,3 → 5 V)
**Czujnik:** fotorezystor (auto-jasność LED)

**Zasilanie:**

```
2S Li-ion (6,4–8,4 V użytkowe)
  └─ koszyk PRZYKLEJONY POD OBUDOWĄ (ładowarka USB-C + balanser na koszyku)
      └─ J1 → SW_PWR → F1 polyfuse → +8V
          ├─ U2  buck 5V (3A Mini DC-DC, Ali) ─► WS2812B  (i nic więcej)
          └─ U1  AP63203 3.3V (SparkFun COM-18357, 2A)
                 ├─ peryferia: nRF24, OLED, gimbale, pull-upy
                 └─ [PMEG2010AEH] ─► VSYS (3,02 V) ─► MP28164 ─► ESP32
```

**WARIANT B — zatwierdzony.** MCU zasilany z railu 3.3V przez diodę,
nie z +5V. Powód: poprowadzenie +5V do MCU na tym layoucie było
nie do przejścia.

Konsekwencje, świadomie zaakceptowane:
- **MCU siedzi na AP63203 (SparkFun), nie na module z Ali.** Moduł 5 V
  zasila wyłącznie diody — nic krytycznego na nim nie wisi.
- **nRF24 dzieli rail z MCU.** To jedyna realna wada wariantu B.
  Bufor: C7 100 µ przy radiu + C37 47 µ i C40 100 n w module + C_VSYS 22 µF.
- MP28164 pracuje w **trybie boost** (2,9–3,0 V → 3,3 V). To dobrze:
  gdyby spadek na diodzie był zerowy, VSYS=3,3 V trafiłby dokładnie
  w mglisty obszar przejścia buck↔boost. Spadek tu pomaga.
- Sprawność łańcucha ~67% zamiast ~75%. Przy 100 mA średnio to 0,05 W,
  czyli ok. 1,5% czasu pracy. Nieistotne.

**Obciążenie AP63203:** peryferia ~180 mA + moduł ~380 mA szczytowo
= ~560 mA wobec 2 A. Zapas 3,5×.

**Próg odcięcia baterii: 6,4 V (3,2 V/ogniwo).**
Zdrowsze dla Li-ion, plus U2 przy 6,0 V miałby tylko 1 V zapasu na dropout.

> **Rozbieżność z firmware:** `include/controller/config.h` ma
> `BATTERY_CELL_EMPTY_MV 6600` (6,6 V pack / 3,3 V na ogniwo), nie 6,4 V
> jak wyżej. Nie poprawiałem tego po cichu — który próg jest aktualny
> (6,4 V z analizy sprzętu, czy 6,6 V zaszyty w kodzie) trzeba ustalić
> i ujednolicić.

Z obudowy wystaje: **USB-C modułu ESP32** (programowanie), antena SMA, okno OLED, okno LDR.

---

## ⚠️ VSYS — dlaczego jest tam dioda

VSYS **nie jest zasilaniem ESP32.** Jest wejściem przetwornicy MP28164,
która dopiero robi 3,3 V. Między VBUS a VSYS **nie ma przetwornicy** —
jest tylko dioda D1.

```
USB-C ──► VBUS ──►│D1│──► VSYS ──► MP28164 ──► 3V3 ──► ESP32
5,1 V         Schottky   4,75 V   buck-boost   3,3 V
                            ▲
                         pin 39
```

Skutek: **przy wetkniętym USB moduł wypycha ~4,75 V na zewnątrz przez pin 39.**

W rev V1 pin 39 był zwarty z railem +3.3V. To oznaczało 4,75 V na:
- nRF24L01+ (abs. max **3,6 V**)
- SSD1306 (abs. max **4,0 V**)
- gimbalach hallotronowych

Czyli zniszczenie radia przy pierwszym programowaniu na złożonej płytce.

**Na stykówce problem nie występuje**, bo VSYS nigdzie nie idzie.
Pojawia się dopiero na PCB. ERC go nie wykryje — to poprawne
połączenie zasilania.

**Rozwiązanie (wariant B, zatwierdzony):**
- usunąć **bezpośrednie** połączenie +3.3V ↔ VSYS
- wstawić w to miejsce diodę: `+3.3V ──[PMEG2010AEH]──► VSYS`
  anoda do +3.3V, katoda do pinu 39
- **22 µF/16 V** ceramik (0805 lub 1206) tuż za diodą przy VSYS

Dioda przewodzi w stronę modułu, blokuje powrót na rail peryferiów.
Przy wetkniętym USB VSYS skacze do 4,75 V, dioda jest spolaryzowana
zaporowo i nRF24 nigdy tego nie zobaczy.

**Bilans napięć.** Przy 2,9–3,0 V na wejściu MP28164 pracuje w trybie
boost, więc prąd wejściowy jest **większy** niż wyjściowy:

```
szczyt ESP32:  300 mA @ 3,3 V
Pout = 3,3 × 0,3          = 0,99 W
Pin  = 0,99 / 0,90        = 1,10 W
Iin  = 1,10 / 2,9         ≈ 380 mA   ← tyle płynie przez diodę
```

| Dioda | Vf @ 380 mA | VSYS | Zapas nad 2,5 V |
|---|---|---|---|
| **PMEG2010AEH** (SOD-123W, 1 A/20 V) | ~0,28 V | **3,02 V** | 520 mV |
| SS14 (SMA) — zamiennik | ~0,40 V | 2,90 V | 400 mV |

Zachowanie po zmianie:

| Sytuacja | VSYS | Kto zasila moduł |
|---|---|---|
| Bateria, USB odłączone | 3,02 V | AP63203 przez diodę |
| Bateria + USB | 4,75 V | USB (dioda zatyka się) |
| Pstryczek OFF + USB | 4,75 V | USB, zero powrotu na rail |

Wszystkie w zakresie MP28164 (2,5–5,5 V).

---

## Architektura firmware

**Środowiska (`platformio.ini`):**

| env | platforma | rola | src filter |
|---|---|---|---|
| `controller_esp32s3_pico` (alias `controller`) | `waveshare_esp32_s3_pico` | nadajnik | `+<controller/>` |
| `rx_test_platform` | Arduino Nano (`nanoatmega328new`, COM10) | odbiornik testowy | `+<receivers/test_platform/>` |
| `rx_test_platform_mega` | Arduino Mega 2560 (COM7) | odbiornik testowy | `+<receivers/test_platform/>` |

Domyślne środowiska budowane razem: `controller_esp32s3_pico, rx_test_platform`.
Nadajnik używa `U8g2` (OLED), `Adafruit NeoPixel` (WS2812B) i `RF24`; odbiorniki
testowe tylko `RF24`.

**Podział `src/controller/`:**

- `hardware_config.h` — jedyne źródło numerów GPIO (mapa pod stykówkę),
  reszta kodu odwołuje się do niego przez `config.h`, nigdy bezpośrednio
  do numerów pinów.
- `config.h` — stałe projektowe: deadzone/expo domyślne, timing wyświetlacza,
  kanał/PA nRF24, próg baterii, `START_SCREEN` do bring-upu.
- `joysticks.cpp` — odczyt ADC, kalibracja (min/center/max per oś),
  deadzone, expo (`powf(norm, 1+expo)`), limit % na oś; wszystko
  persystowane przez `storage`.
- `buttons.cpp` — debounce, `keyShortClick`/`keyLongPress`/`keyReleased`
  dla 7 przycisków fizycznych + 2 przyciski gimbali.
- `leds.cpp` — sterowanie 5× WS2812B (Adafruit NeoPixel), tryb manual
  override do testu LED w UI.
- `photo_sensor.cpp` — odczyt fotorezystora, mapowanie na jasność LED
  (`photoSensorLedBrightnessPct`), konfigurowalny filtr/histereza.
- `battery.cpp` — pomiar baterii **nadajnika** (dzielnik na `BATTERY_PIN`),
  uśrednianie 8 próbek, mapowanie mV→% (liniowe, patrz TODO).
- `storage.cpp` — cienka warstwa nad ESP32 `Preferences` (NVS),
  namespace `"flexrc"`, blob-y z magic+CRC (kalibracja, deadzone, expo, limit).
- `control_link.cpp` — logika arming/failsafe po stronie UI: F1 (long-press)
  włącza/wyłącza link, F2 (long-press) uzbraja sterowanie, tylko gdy
  `ReceiverLinkState::Connected`; auto-disarm przy zejściu z ekranu głównego
  albo utracie linku.
- `receiver.cpp` — maszyna stanów łącza radiowego (`Idle → Connecting →
  Connected → Lost/RadioError`), TX co 20 ms (50 Hz), odbiór telemetrii
  baterii **odbiornika** przez ACK payload, filtr mediana-z-3 + EMA,
  failsafe czasowy `RX_TIMEOUT_MS=120 ms` (przejście w `Lost`).
- `tx_frame.cpp` — buduje `CommFrame` z bieżących odczytów gimbali/przycisków;
  zwraca ramkę zerową, gdy sterowanie nie jest dozwolone (nie uzbrojone /
  poza main loop / w kalibracji).
- `main.cpp` — spina wszystko w `setup()`/`loop()`; `loop()` jest non-blocking
  poza ekranami kalibracji/expo (te celowo blokują `controlLinkTick`/`receiverLoop`).
- `ui/` — `menu.cpp` (stan maszyny UI: Main/Settings/JoyCalibration/Expo/
  LedTest/PhotoSettings/IoReadings) + `loop_main`, `loop_settings` i strony
  ustawień (`calib_joy`, `set_expo`, `led_test`, `set_photo`, `io_readings`).

**`lib/common`** — kod dzielony między nadajnikiem a odbiornikami (kompilowany
w obie strony, rozdzielony przez `ROLE_CONTROLLER`/`ROLE_RECEIVER`):
- `comm.h/.cpp` — cała logika radiowa (patrz niżej).
- `time_utils.h/.cpp` — helper `everyMs()` do nieblokującego taktowania pętli.

**Protokół radiowy (`lib/common/src/comm.cpp`):**

- Ramka aplikacyjna `CommFrame` (wspólna) → na eterze pakowana do
  `TxPkt` (`#pragma pack(1)`, **5 bajtów**: `lx, ly, rx, ry` jako `int8_t`
  -100..100, `joyButtons` bit0=JL/bit1=JR) i zwrotnego `AckPkt`
  (**2 bajty**: `battPct` + `flags` zarezerwowane).
- Telemetria (bateria **odbiornika**) jedzie w drugą stronę przez
  **ACK payload** nRF24 (`enableAckPayload` / `writeAckPayload`) —
  brak osobnej ramki zwrotnej.
- Kanał `76`, `RF24_250KBPS` (najbardziej odporny), `RF24_CRC_16`,
  `setRetries(3, 5)` (delay ×250 µs, 5 prób), `setAutoAck(true)`,
  `setPayloadSize(sizeof(TxPkt))`.
- Adres pipe: `"RC001"`, ten sam kanał zapisu (TX) i odczytu pipe 1 (RX).
- `NRF_PA_LEVEL 0` (`RF24_PA_MIN`) w `controller/config.h` **i** w
  `receivers/test_platform/config.h` — jawnie skomentowane jako ustawienie
  **bring-up**; moduł ma PA/LNA, docelowo trzeba podnieść (patrz TODO / B6).

---

## ⚠️ Mapa pinów — DWIE rewizje sprzętu

Piny zostały **celowo przemapowane** na etapie projektu PCB, żeby ograniczyć
krzyżowania ścieżek. Firmware (`include/controller/hardware_config.h`)
odpowiada stykówce. Schemat/PCB odpowiada tabeli z sekcji sprzętowej poniżej.
**Żadna wersja nie jest błędem** — to dwie rewizje tego samego urządzenia.

Sprawdzone **wszystkie** sygnały (nie tylko SPI) — jedyna faktyczna
rozbieżność to zamiana MOSI/MISO na SPI:

| Sygnał | GPIO — firmware (stykówka) | GPIO — schemat (PCB, miedź) | Zgodne? |
|---|---|---|---|
| Joystick R — oś Y (`RY`) | GP1 | GP1 | ✅ |
| Joystick R — oś X (`RX`) | GP2 | GP2 | ✅ |
| Joystick L — oś Y (`LY`) | GP4 | GP4 | ✅ |
| Joystick L — oś X (`LX`) | GP5 | GP5 | ✅ |
| Przycisk gimbala R | GP6 | GP6 | ✅ |
| Przycisk gimbala L | GP7 | GP7 | ✅ |
| Fotorezystor | GP8 | GP8 | ✅ |
| Pomiar baterii (nadajnik) | GP9 | GP9 | ✅ |
| SW_UP | GP11 | GP11 | ✅ |
| SW_LEFT | GP12 | GP12 | ✅ |
| SW_CENTER | GP13 | GP13 | ✅ |
| SW_RIGHT | GP14 | GP14 | ✅ |
| SW_DOWN | GP15 | GP15 | ✅ |
| I2C SCL (OLED) | GP17 | GP17 | ✅ |
| I2C SDA (OLED) | GP18 | GP18 | ✅ |
| nRF24 SCK | GP34 | GP34 | ✅ |
| nRF24 MISO | GP35 | GP35 (fizycznie MOSI) | ❌ zamienione |
| nRF24 MOSI | GP36 | GP36 (fizycznie MISO) | ❌ zamienione |
| nRF24 CSN | GP37 | GP37 | ✅ |
| nRF24 CE | GP38 | GP38 | ✅ |
| WS2812B DIN | GP39 | GP39 | ✅ |
| SW_F2 | GP41 | GP41 | ✅ |
| SW_F1 | GP42 | GP42 | ✅ |

Firmware deklaruje `NRF_MOSI_PIN=36` / `NRF_MISO_PIN=35`; na PCB fizycznie
GP35=MOSI, GP36=MISO. Trzeba to uwzględnić przy przechodzeniu ze stykówki
na PCB (patrz FAZA 3 / B6 niżej) — jedna zamieniona para, tak jak
podejrzewano, reszta 22 sygnałów zgodna 1:1.

**Wolne GPIO:** GP10 (ADC1_CH9), GP16, GP33, GP40

---

## Czego ERC i DRC NIE sprawdzają

ERC i DRC przechodzą czysto. To potwierdza, że schemat jest spójny
**sam ze sobą** — nie że jest poprawny. Cała FAZA 1 leży poza ich zasięgiem:

| Problem | ERC/DRC złapie? |
|---|---|
| VSYS zwarty z railem 3.3V | Nie — poprawne połączenie zasilania |
| Dzielnik BATT daje 3,18 V | Nie — ERC nie liczy napięć |
| R31 po złej stronie bufora | Nie — połączenie jest legalne |
| Footprint nie pasuje do modułu | Nie — DRC sprawdza geometrię |
| C7/C8 daleko od nRF24 | Nie |

Te punkty trzeba przejść ręcznie.

**Osobna weryfikacja przed zamówieniem:** czy numeracja padów footprintu
nRF24 zgadza się z numeracją pinów symbolu i z fizycznym modułem.
Standard breakoutu: `1 GND, 2 VCC, 3 CE, 4 CSN, 5 SCK, 6 MOSI, 7 MISO, 8 IRQ`.
Netlista `.kicad_pcb` tego nie rozstrzygnie — jest generowana ze schematu,
więc powtórzy ten sam obraz. Trzeba porównać footprint z fizyczną częścią.

---

## FAZA 1 — Poprawki schematu (przed zamówieniem PCB)

### 1A. Zmiany wymagające przeróbki miedzi

- [ ] **VSYS — wstawić diodę: `+3.3V ──[PMEG2010AEH]──► VSYS`**
      Szczegóły i uzasadnienie: sekcja „VSYS — dlaczego jest tam dioda".
      Footprint: `Diode_SMD:D_SOD-123`. Zamiennik: SS14 (`Diode_SMD:D_SMA`),
      kosztem 120 mV zapasu.
      **Anoda → +3.3V, katoda → pin 39.**
      Prąd przez diodę do 380 mA szczytowo (tryb boost MP28164).

- [ ] **22 µF/16 V ceramik przy VSYS, za diodą**
      Moduł ma własne C37 47 µ i C40 100 n, ale dioda wnosi impedancję
      między nie a rail. 22 µF (zamiast wyliczonych 10 µF) świadomie
      z zapasem — kondensator jest tańszy niż druga rewizja płytki.

- [ ] **Zweryfikować, że +5V nie idzie nigdzie poza WS2812B**
      Po przejściu na wariant B rail 5 V obsługuje wyłącznie diody
      i 74AHCT1G125. Upewnić się, że nie został żaden ślad po
      wcześniejszym pomyśle prowadzenia +5V do MCU.

- [ ] **R1: 33k → 47k** (dzielnik BATT)
      Obecnie przy 8,4 V: `8,4 × 20,1/53,1 = 3,18 V` — powyżej full-scale
      ADC (~3,1 V przy 12 dB), obcięcie przy pełnym pakiecie.
      Po zmianie: `8,4 × 20,1/67,1 = 2,52 V`, zakres liniowy.
      W firmware: `BATTERY_DIVIDER_R_TOP_OHM` 33000 → 47000
      (obecnie w `config.h` wciąż 33000 — zmiana czeka na rewizję PCB).

- [ ] **R31 220R — przenieść na wyjście 74AHCT1G125**
      Teraz siedzi na wejściu bufora, gdzie nie robi nic (bramka CMOS
      nie pobiera prądu). Terminacja szeregowa ma być między pinem 4
      a DIN diody D1.

- [ ] **J1 — kluczowany JST-XH + zabezpieczenie odwrotnej polaryzacji**
      Pakiet wchodzi luźnym przewodem z zewnątrz obudowy.
      P-MOSFET jako ideal diode.

- [ ] **(opcjonalnie) nRF24 IRQ → GP16**
      **Nie jest potrzebne.** Przy 250 kbps transakcja trwa ~1 ms,
      a pętla ma 20 ms — polling zjada 5% okna. IRQ ma sens dopiero
      przy 200+ Hz, przy trybach sleep, albo na odbiorniku (jitter
      serwa). Dołóż tylko, jeśli ścieżka wchodzi bez psucia routingu.

**USUNIĘTE z listy:**
- ~~przycisk reset na własnym PCB~~ — moduł ma Key1 (BOOT) i Key2 (RESET).
  Zamiast tego: **zaplanować w obudowie dostęp do obu przycisków modułu.**
  Sam RUN nie wystarczy, bo GPIO0 nie jest na listwie, a bez BOOT-a nie
  wymusisz trybu download, gdy auto-program zawiedzie.
- ~~sprawdzić obciążalność U2~~ — **nieaktualne przy wariancie B.** U2 zasila
  już tylko WS2812B (do 300 mA) wobec 3 A. Zapas 10×.
- ~~zmierzyć napięcie wyjściowe U2~~ — nieistotne, odkąd U2 nie zasila MCU.

### 1B. Porządki w schemacie (miedź bez zmian)

- [ ] Poprawić etykiety: `SDA=GP18`, `SCL=GP17`, `MOSI=GP35`, `MISO=GP36`
- [ ] Poprawić nazwy netów gimbali — `JOY_L_1` ma sygnały nazwane
      `RX / RY / JOY R SW`. Ustalić, który gimbal jest fizycznie lewy,
      i ujednolicić.
- [ ] Przejrzeć **pozostałe** nety — trzy pomyłki tego samego typu
      sugerują, że mogło być ich więcej
- [ ] Sprawdzić duplikat referencji `F1` (Polyfuse) — annotate + ERC
- [ ] Sprawdzić rating polyfuse względem prądu rozruchowego bucków
- [ ] Przelicieć ERC na czysto, bump rev → V2

### 1C. Layout

- [ ] `C7 100u` i `C8 100n` **fizycznie przy pinach VCC/GND nRF24**
      — nie „gdzieś na railu". Najczęstsza przyczyna „radio działa na 3 m".
- [ ] DRC + wizualna kontrola 3D

### 1D. Do sprawdzenia na zmontowanej płytce (bring-up)

- [ ] **Oscyloskop na VSYS, pierwsze 10 ms po pstryknięciu.**
      Moduł przy rozruchu ładuje ~69 µF (C37 47 µ + C_VSYS 22 µ) przez
      diodę i chwilowo ściągnie VSYS w dół. AP63203 ma soft-start
      i 2 A zapasu, więc powinno być czysto.
      **Próg alarmowy: zapad poniżej ~2,6 V.** Gdyby wystąpił —
      zwiększyć C_VSYS albo obniżyć Vf (wymiana na diodę o niższym spadku).

- [ ] **Zmierzyć VSYS w normalnej pracy.** Oczekiwane ~3,02 V.
      Poniżej 2,7 V oznacza, że spadek na diodzie jest większy niż
      zakładany — sprawdzić rzeczywisty prąd wejściowy modułu.

- [ ] **Zasięg radia po podniesieniu PA_LEVEL.** Wariant B stawia nRF24
      na wspólnym railu z MCU. Jeśli zasięg odbiega od oczekiwań dla
      PA/LNA, pierwszym podejrzanym jest sprzężenie przez rail 3.3V —
      sprawdzić ripple na VCC radia przy TX.

---

## FAZA 2 — Obudowa (Fusion 360)

- [ ] Eksport PCB z KiCada do **STEP**
- [ ] Import STEP do Fusion jako referencja wymiarowa
- [ ] Model korpusu wokół realnej płytki
- [ ] **Wydruk makiety bez elektroniki** — kontrola ergonomii w dłoni
- [ ] Dopiero po makiecie: ewentualne poprawki PCB
- [ ] Otwory: USB-C modułu, okno OLED, okno LDR (z przegrodą przed
      światłem własnych diod RGB), przepust SMA, przepust przewodu 2S
- [ ] Mocowanie koszyka 2S pod spodem
- [ ] Rozważyć mosiężny panel przedni — waga: mosiądz ~8,5 g/cm³,
      panel 150×100×2 mm ≈ **255 g**. Korpus w całości z blachy 2 mm
      to 600–900 g, za dużo na trzymany w rękach nadajnik.

---

## FAZA 3 — Firmware

- [ ] **`git tag breadboard-working` PRZED jakąkolwiek zmianą pinów**
      To jedyny punkt odniesienia „tu wszystko działało". Bez niego,
      gdy PCB nie ruszy, nie odróżnisz błędu przemapowania od wady sprzętu.
- [ ] **Pełne przemapowanie stykówka → PCB**, nie punktowa łatka na MOSI/MISO.
      Przejść całą tabelę pinów, sygnał po sygnale (patrz „Mapa pinów" wyżej —
      w praktyce to jedna para: SPI MOSI/MISO).
- [ ] Po przemapowaniu: podnieść `NRF_PA_LEVEL` z PA_MIN — moduł ma PA/LNA,
      bring-up na minimum był świadomy, ale docelowo trzeba więcej.

Stan reszty punktów z poprzedniej listy, zweryfikowany w kodzie:

- [x] Przepisać mapę pinów wg tabeli (stykówka) — `hardware_config.h` gotowe.
- [x] Kalibracja gimbali (min/center/max, deadband, expo) — `joysticks.cpp`
      + strony UI `calib_joy`/`set_expo`, persystencja w NVS.
- [ ] Oversampling ADC — **zrobione tylko dla baterii** (`BATTERY_AVG_SAMPLES=8`
      w `battery.cpp`). Gimbale (`joysticks.cpp`) czytają pojedynczy
      `analogRead()` na cykl, bez uśredniania/oversamplingu.
- [~] Pomiar baterii z uśrednianiem + krzywa 2S Li-ion — uśrednianie jest
      (`battery.cpp`, 8 próbek), ale mapowanie mV→% jest **liniowe**
      między `BATTERY_CELL_EMPTY_MV`/`FULL_MV`, nie prawdziwą krzywą
      rozładowania 2S Li-ion (która jest mocno nieliniowa).
- [~] Protokół nRF24: kanały, retry, auto-ACK — gotowe w `comm.cpp`
      (kanał 76, retries 3/5, CRC16, autoACK, ACK payload).
      Failsafe: gotowe po stronie **nadajnika** (link state machine +
      arming w `receiver.cpp`/`control_link.cpp`, timeout 120 ms).
      **Brak po stronie odbiornika** — `rx_test_platform` nie ma
      timeoutu na `lastRxAt`; przy utracie linku silniki trzymają
      ostatnią odebraną wartość PWM zamiast się wyzerować.
- [x] UI na OLED — gotowe: `menu.cpp` + strony ustawień (kalibracja,
      expo, test LED, czułość fotorezystora, podgląd I/O).
- [x] Auto-jasność LED z LDR — gotowe (`photoSensorLedBrightnessPct`,
      używane w `receiver.cpp` i `control_link.cpp`).
- [ ] Odbiornik: wyjścia PWM/PPM/SBUS + **failsafe przy utracie linku** —
      tylko PWM (2 kanały: steering z `rx`, throttle z `ly`) w
      `src/receivers/test_platform/main.cpp`. PPM/SBUS nie istnieją.
      Failsafe nie istnieje (patrz punkt wyżej).
- [ ] **(z `do_zrobienia.txt`)** LED/wskaźnik na odbiorniku sterowany
      wysterowaniem kanałów analogowych — pomysł nowy, jeszcze niezrobiony.

---

## Kolejność

**Nie zamawiaj PCB przed ukończeniem FAZY 1 i punktu „wydruk makiety" z FAZY 2.**
Makieta jest tania i wyłapie większość błędów ergonomii, których nie widać w CAD.

# FlexRC

Kontroler RC (nadajnik + odbiornik) na ESP32-S3. Autor: Luke / Flex Electronics.
Repo: https://github.com/LukeSky91/FlexRC

**Stan (2026-11-08):**
- Schemat i PCB **rev V1** (pliki KiCad, rev nie podbita do V2). Większość
  poprawek z FAZY 1A jest już naniesiona na miedź (dioda VSYS, C_VSYS,
  R31 za buforem, dzielnik BATT 51k, P-MOSFET na wejściu). **NIE zamówione.**
- Firmware nadajnika jest **przemapowany na piny PCB rev V1**
  (`hardware_config.h` wg netlisty) i się kompiluje, ale **na PCB jeszcze
  nie uruchomiony** (płytki brak). **Stykówka nie jest już wspierana** —
  ostatni zweryfikowany na stykówce stan to `git tag breadboard-working`.
- Firmware ma pełne UI OLED ze stronami ustawień, maszynę stanów łącza
  radiowego z armingiem/failsafe i telemetrię zwrotną (bateria odbiornika)
  przez ACK payload nRF24.
- Odbiornik (`rx_test_platform`) jest celowo prosty: PWM na 2 kanały,
  telemetria baterii, **bez failsafe przy utracie linku**.
- Obudowa: STEP płytki wyeksportowany (`PCB/FlexRC/FlexRC.step`), model
  obudowy jeszcze niezrobiony.

---

## Narzędzia

| Obszar | Narzędzie |
|---|---|
| Firmware | VSCode + PlatformIO |
| Schemat / PCB | KiCad 10.0 (wg `generator_version` w plikach) |
| Obudowa | Fusion 360 (druk 3D, ew. mosiężny panel przedni) |

---

## Sprzęt

**MCU:** Waveshare ESP32-S3-Pico (23803), układ ESP32-S3R2
— 16 MB flash (W25Q128), 2 MB PSRAM **quad** (GPIO35/36/37 wolne — wersje
  oktalne by je zabrały; na PCB używane przez SPI nRF24)
— wewnętrzny regulator: **MP28164 buck-boost**, 2 A, wejście = pin VSYS (39)
— zakres wejścia VSYS: **2,5–5,5 V**
— **D1 (na module) = MBR230LSFT1G**, Schottky VBUS → VSYS (topologia Pico)
— przyciski na module: **Key1 = GPIO0/BOOT**, **Key2 = CHIP_PU/RESET**
— **GPIO0 NIE jest wyprowadzony na listwę 40-pin** (tylko Key1 + testpoint P3).
  RUN jest dostępny na pinie 30 (na PCB niepodłączony).

**Zajęte przez moduł — NIE UŻYWAĆ:**
- `GPIO3` → USB_ADC (dzielnik R2/R4 100K od VBUS)
- `GPIO21` → wbudowana dioda WS2812B modułu
- `GPIO19 / GPIO20` → USB D− / D+
- `GPIO0` → przycisk BOOT, niedostępny na listwie

**Radio (TX1):** nRF24L01+ **PA/LNA** (antena zewnętrzna SMA — obudowa metalowa jest OK)
**Wyświetlacz (DS1):** 128×64 I2C. Footprint/symbol na PCB nazywa się
`SSD1306_128x64`, ale **firmware używa sterownika SH1106**
(`U8G2_SH1106_128X64_NONAME_1_HW_I2C` w `display.cpp`) — działało tak na
stykówce. Ustalić, jaki sterownik ma docelowy moduł, i ujednolicić opis.
**Wejścia:** 2 gimbale hallotronowe (JOY_L_1, JOY_R_1, z przyciskami),
7 przycisków (SW_U1/D1/L1/R1/C1 + SW_F1/F2), każdy z pull-upem 10k i 1k
szeregowo.
**LED:** 5 × WS2812B (D1..D5) na +5V, przez U3 74AHCT1G125 (level shifter
3,3 → 5 V) i R31 330R na wyjściu bufora.
**Czujnik:** fotorezystor R9 (auto-jasność LED).

**Zasilanie (wg netlisty PCB rev V1):**

```
2S Li-ion (6,4–8,4 V użytkowe)
  └─ koszyk PRZYKLEJONY POD OBUDOWĄ (ładowarka USB-C + balanser na koszyku)
      └─ J1 (pady na przewód, raster 2,54) → Q1 P-MOSFET (rev. polarity, D od baterii)
          → SW_PWR_1 → F1 polyfuse (Bourns MF-RG300, 3 A hold) → +8V
          ├─ U2  buck 5V (3A Mini DC-DC, Ali) ─► WS2812B + U3  (i nic więcej)
          └─ U1  AP63203 3.3V (SparkFun COM-18357, 2A)
                 ├─ peryferia: nRF24, OLED, gimbale, pull-upy
                 └─ DP1 B140-E3 ─► VSYS (~2,9 V) ─► MP28164 ─► ESP32
                                    └─ CP1 22 µF do GND
```

Sprawdzone w netliście: na +5V wiszą tylko U2, U3, D1..D5, C23 i C24..C28
(odsprzęganie diod) oraz TP3. Nic z +5V nie idzie do MCU.

**WARIANT B — zatwierdzony i naniesiony na PCB.** MCU zasilany z railu 3.3V
przez diodę, nie z +5V. Powód: poprowadzenie +5V do MCU na tym layoucie było
nie do przejścia.

Konsekwencje, świadomie zaakceptowane:
- **MCU siedzi na AP63203 (SparkFun), nie na module z Ali.** Moduł 5 V
  zasila wyłącznie diody — nic krytycznego na nim nie wisi.
- **nRF24 dzieli rail z MCU.** To jedyna realna wada wariantu B.
  Bufor na +3.3V: C7 100 µ + C8 100 n + C10 10 n (okolice radia), C3 220 µ,
  C5 100 µ; w module C37 47 µ i C40 100 n; CP1 22 µF przy VSYS.
- MP28164 pracuje w **trybie boost** (~2,9 V → 3,3 V). To dobrze:
  gdyby spadek na diodzie był zerowy, VSYS=3,3 V trafiłby dokładnie
  w mglisty obszar przejścia buck↔boost. Spadek tu pomaga.
- Sprawność łańcucha ~67% zamiast ~75%. Przy 100 mA średnio to 0,05 W,
  czyli ok. 1,5% czasu pracy. Nieistotne.

**Obciążenie AP63203:** peryferia ~180 mA + moduł ~380 mA szczytowo
= ~560 mA wobec 2 A. Zapas 3,5×.

**Próg odcięcia baterii: 6,4 V (3,2 V/ogniwo)** wg analizy sprzętu.
Zdrowsze dla Li-ion, plus U2 przy 6,0 V miałby tylko 1 V zapasu na dropout.

> **Rozbieżność z firmware (nierozstrzygnięta):** `include/controller/config.h`
> ma `BATTERY_CELL_EMPTY_MV 6600` (6,6 V pack / 3,3 V na ogniwo), nie 6,4 V.
> Trzeba ustalić, który próg jest aktualny, i ujednolicić.

Z obudowy wystaje: **USB-C modułu ESP32** (programowanie), antena SMA, okno OLED, okno LDR.

---

## ⚠️ VSYS — dlaczego jest tam dioda

VSYS **nie jest zasilaniem ESP32.** Jest wejściem przetwornicy MP28164,
która dopiero robi 3,3 V. Między VBUS a VSYS **nie ma przetwornicy** —
jest tylko dioda D1 na module.

```
USB-C ──► VBUS ──►│D1│──► VSYS ──► MP28164 ──► 3V3 ──► ESP32
5,1 V         Schottky   4,75 V   buck-boost   3,3 V
                            ▲
                         pin 39
```

Skutek: **przy wetkniętym USB moduł wypycha ~4,75 V na zewnątrz przez pin 39.**

We wcześniejszym layoucie pin 39 był zwarty z railem +3.3V. To oznaczałoby
4,75 V na nRF24L01+ (abs. max **3,6 V**), wyświetlaczu (abs. max ~4,0 V)
i gimbalach — zniszczenie radia przy pierwszym programowaniu na złożonej
płytce. ERC tego nie wykryje — to poprawne połączenie zasilania.

**Stan na PCB (naniesione):**
- `+3.3V ──[DP1 B140-E3, SMA]──► VSYS` — anoda (pad 2) do +3.3V,
  katoda (pad 1) do `Net-(A1-VSYS)` ✅
- `CP1 22 µF` (1206) z VSYS do GND ✅

Dioda przewodzi w stronę modułu, blokuje powrót na rail peryferiów.
Przy wetkniętym USB VSYS skacze do 4,75 V, dioda jest spolaryzowana
zaporowo i nRF24 tego nie zobaczy.

**Bilans napięć.** Przy ~2,9 V na wejściu MP28164 pracuje w trybie
boost, więc prąd wejściowy jest **większy** niż wyjściowy:

```
szczyt ESP32:  300 mA @ 3,3 V
Pout = 3,3 × 0,3          = 0,99 W
Pin  = 0,99 / 0,90        = 1,10 W
Iin  = 1,10 / 2,9         ≈ 380 mA   ← tyle płynie przez diodę
```

| Dioda | Vf @ 380 mA | VSYS | Zapas nad 2,5 V |
|---|---|---|---|
| **B140-E3** (SMA, 1 A/40 V) — **zamontowana na PCB** | ~0,4 V | **~2,9 V** | ~400 mV |
| PMEG2010AEH (SOD-123W, 1 A/20 V) — pierwotny plan | ~0,28 V | 3,02 V | 520 mV |

B140 to wariant „SS14-podobny”: ~120 mV mniej zapasu niż PMEG, nadal
w zakresie. Jeśli bring-up (1D) pokaże zbyt niskie VSYS — wymiana na PMEG
wymaga innego footprintu (SOD-123W zamiast SMA).

Zachowanie:

| Sytuacja | VSYS | Kto zasila moduł |
|---|---|---|
| Bateria, USB odłączone | ~2,9 V | AP63203 przez diodę |
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
testowe tylko `RF24`. Build nadajnika: `pio run -e controller`.

**Podział `src/controller/`:**

- `include/controller/hardware_config.h` — jedyne źródło numerów GPIO
  (mapa wg PCB rev V1, komentarze z oznaczeniami elementów z PCB). Reszta
  kodu odwołuje się do niego przez `config.h`, nigdy bezpośrednio do
  numerów pinów.
- `include/controller/config.h` — stałe projektowe: deadzone/expo domyślne,
  timing wyświetlacza, kanał/PA nRF24, dzielnik i progi baterii,
  `START_SCREEN` do bring-upu.
- `joysticks.cpp` — odczyt ADC, kalibracja (min/center/max per oś),
  deadzone, expo (`powf(norm, 1+expo)`), limit % na oś; wszystko
  persystowane przez `storage`. **Kalibracja zapisana w NVS pochodzi
  ze stykówki (inne piny osi) — na PCB trzeba skalibrować od nowa.**
- `buttons.cpp` — debounce, `keyShortClick`/`keyLongPress`/`keyReleased`
  dla 7 przycisków fizycznych + 2 przyciski gimbali.
- `leds.cpp` — sterowanie 5× WS2812B (Adafruit NeoPixel), tryb manual
  override do testu LED w UI.
- `photo_sensor.cpp` — odczyt fotorezystora, mapowanie na jasność LED
  (`photoSensorLedBrightnessPct`), konfigurowalny filtr/histereza.
- `battery.cpp` — pomiar baterii **nadajnika** (dzielnik R1 51k / R2+R3 20k
  na `BATTERY_PIN`), uśrednianie 8 próbek, mapowanie mV→% (liniowe, patrz TODO).
- `storage.cpp` — cienka warstwa nad ESP32 `Preferences` (NVS),
  namespace `"flexrc"`, blob-y z magic+CRC (kalibracja, deadzone, expo, limit).
- `control_link.cpp` — logika arming/failsafe po stronie UI: F1 (long-press)
  włącza/wyłącza link, F2 (long-press) uzbraja sterowanie, tylko gdy
  `ReceiverLinkState::Connected`; auto-disarm przy zejściu z ekranu głównego
  albo utracie linku.
- `receiver.cpp` — maszyna stanów łącza radiowego (`Idle → Connecting →
  Connected → Lost/RadioError`), TX co 20 ms (50 Hz, harmonogram
  `lastTxMs = now` — dryfuje przy spóźnieniu), odbiór telemetrii baterii
  **odbiornika** przez ACK payload, filtr mediana-z-3 + EMA, failsafe
  czasowy `RX_TIMEOUT_MS=120 ms` (przejście w `Lost`). `BATTERY_DEBUG`
  i `LINK_DEBUG` są domyślnie **włączone** (Serial).
- `tx_frame.cpp` — buduje `CommFrame` z bieżących odczytów gimbali/przycisków;
  zwraca ramkę zerową, gdy sterowanie nie jest dozwolone (nie uzbrojone /
  poza main loop / w kalibracji).
- `display.cpp` — U8g2, SH1106 w trybie page buffer (`_1_`), I2C 400 kHz,
  limiter `DISPLAY_MIN_FLUSH_INTERVAL_MS=50`, odzyskiwanie po błędzie I2C.
- `main.cpp` — spina wszystko w `setup()`/`loop()`. Wszystko w jednym
  wątku: `menuLoop` → `controlLinkTick` → `receiverLoop` → `ledsShow`
  (co 20 ms) → `displayTick`. **`loop()` nie jest w pełni nieblokująca:**
  pełny flush OLED po I2C trwa ~25 ms i w tym czasie TX czeka (jitter TX
  do ~25 ms). Dodatkowo ekrany kalibracji/expo celowo blokują
  `controlLinkTick`/`receiverLoop`.
- `ui/` — `menu.cpp` (stan maszyny UI: Main/Settings/JoyCalibration/Expo/
  LedTest/PhotoSettings/IoReadings) + `loop_main`, `loop_settings` i strony
  ustawień (`calib_joy`, `set_expo`, `led_test`, `set_photo`, `io_readings`).
  `io_readings` pokazuje surowe odczyty z etykietami GPIO/ADC zgodnymi z PCB.

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
  `setRetries(3, 5)` (delay ×250 µs, 5 prób — blokujący `write()` do ~6 ms
  przy braku ACK), `setAutoAck(true)`, `setPayloadSize(sizeof(TxPkt))`.
- SPI: `SPI.begin(NRF_SCK_PIN, NRF_MISO_PIN, NRF_MOSI_PIN, NRF_CSN_PIN)` —
  piny z `hardware_config.h`.
- Adres pipe: `"RC001"`, ten sam kanał zapisu (TX) i odczytu pipe 1 (RX).
- `NRF_PA_LEVEL 0` (`RF24_PA_MIN`) w `include/controller/config.h` **i** w
  `include/receivers/test_platform/config.h` — ustawienie **bring-up**;
  moduł ma PA/LNA, docelowo trzeba podnieść (FAZA 3).

**Odbiornik testowy (`src/receivers/test_platform/main.cpp`):**
- Wyjścia przez `analogWrite()` — PWM sprzętowy AVR (kilkaset Hz, 8 bit),
  **nie** sygnał serwa/ESC (50 Hz, 1–2 ms). `pwmFromAxis()` bierze
  **wartość bezwzględną** osi — kierunek ginie (−100 i +100 dają to samo).
- Brak failsafe: `lastRx` trzyma ostatnią ramkę bez względu na `lastRxAt`.

---

## Mapa pinów — PCB rev V1

**Źródło prawdy:** netlista `PCB/FlexRC/FlexRC.kicad_pcb` (nazwy netów na
padach A1 = ESP32-S3-Pico). Firmware (`include/controller/hardware_config.h`)
jest zgodny z tą tabelą. **Stykówka nie jest już wspierana** — ostatni stan
pod stykówkę: `git tag breadboard-working`.

| Sygnał | GPIO | Net / element na PCB |
|---|---|---|
| Gimbal L — oś Y (`LY`) | GP1 | `CH0` ← JOY_L_1 Y2 |
| Gimbal L — oś X (`LX`) | GP2 | `CH1` ← JOY_L_1 X2 |
| Gimbal L — przycisk | GP4 | `CH3` ← JOY_L_1 SW (R29 1k, pull-up R27) |
| Gimbal R — oś Y (`RY`) | GP5 | `CH4` ← JOY_R_1 Y2 |
| Gimbal R — oś X (`RX`) | GP6 | `CH5` ← JOY_R_1 X2 |
| Gimbal R — przycisk | GP7 | `CH6` ← JOY_R_1 SW (R30 1k, pull-up R28) |
| Fotorezystor | GP8 | `CH7` ← R9/R10/R11 przez R12 |
| Pomiar baterii (nadajnik) | GP9 | `CH8` ← dzielnik R1 51k / R2+R3 20k, przez R4 |
| SW_DOWN | GP11 | SW_D1 (R17 / R24) |
| SW_CENTER | GP12 | SW_C1 (R15 / R22) |
| SW_RIGHT | GP13 | SW_R1 (R16 / R23) |
| SW_LEFT | GP14 | SW_L1 (R14 / R21) |
| SW_UP | GP15 | SW_U1 (R13 / R20) |
| I2C SCL (OLED) | GP17 | R7 33R → DS1 SCL, pull-up R5 5k1 |
| I2C SDA (OLED) | GP18 | R8 33R → DS1 SDA, pull-up R6 5k1 |
| nRF24 CE | GP34 | TX1 pad 3 |
| nRF24 SCK | GP35 | TX1 pad 5 |
| nRF24 MISO | GP36 | TX1 pad 7 |
| nRF24 MOSI | GP37 | TX1 pad 6 |
| nRF24 CSN | GP38 | TX1 pad 4 |
| WS2812B DIN | GP39 | U3 74AHCT1G125 → R31 330R → D1..D5 |
| SW_F2 | GP41 | SW_F2 (R18 / R25) |
| SW_F1 | GP42 | SW_F1 (R19 / R26) |

**Wolne GPIO (niepodłączone na PCB):** GP10 (ADC1_CH9), GP16, GP33, GP40.
**Planowane (FAZA 1A, złącze OLED I2C/SPI):** GP16 = OLED CS, GP33 = OLED DC, GP40 = OLED RES — wtedy wolne tylko GP10.
nRF24 IRQ (TX1 pad 8) niepodłączony.

---

## Czego ERC i DRC NIE sprawdzają

ERC/DRC potwierdzają, że schemat jest spójny **sam ze sobą** — nie że jest
poprawny. Typowe problemy poza ich zasięgiem (z tego projektu):

| Problem | ERC/DRC złapie? | Stan w rev V1 |
|---|---|---|
| VSYS zwarty z railem 3.3V | Nie — poprawne połączenie zasilania | ✅ naprawione (DP1) |
| Dzielnik BATT powyżej full-scale ADC | Nie — ERC nie liczy napięć | ✅ naprawione (R1 51k) |
| R31 po złej stronie bufora | Nie — połączenie jest legalne | ✅ naprawione |
| Odwrotnie wstawiony P-MOSFET zabezpieczenia | Nie | ✅ naprawione (Q1) |
| Footprint nie pasuje do modułu | Nie — DRC sprawdza geometrię | do sprawdzenia |
| C7/C8 daleko od nRF24 | Nie | ⚠️ patrz 1C |

**Osobna weryfikacja przed zamówieniem:** czy numeracja padów footprintu
nRF24 zgadza się z fizycznym modułem. Footprint TX1 ma standard breakoutu:
`1 GND, 2 VCC, 3 CE, 4 CSN, 5 SCK, 6 MOSI, 7 MISO, 8 IRQ`. Netlista tego
nie rozstrzygnie — trzeba porównać footprint z fizyczną częścią.

---

## FAZA 1 — Poprawki schematu (przed zamówieniem PCB)

### 1A. Zmiany wymagające przeróbki miedzi

- [x] **VSYS — dioda `+3.3V ──[DP1]──► VSYS`** — naniesiona, ale jako
      **B140-E3 (SMA)**, nie PMEG2010AEH. Anoda → +3.3V, katoda → pin 39 ✅.
      VSYS ~2,9 V zamiast 3,02 V; zapas ~400 mV nad 2,5 V. Do potwierdzenia
      pomiarem (1D).

- [x] **CP1 22 µF przy VSYS, za diodą** — jest (1206). Sprawdzić napięcie
      znamionowe w BOM (plan: 16 V).

- [x] **+5V nie idzie nigdzie poza WS2812B** — sprawdzone w netliście:
      tylko U2, U3, D1..D5, C23..C28, TP3.

- [x] **R1 = 51k** (dzielnik BATT; dół R2+R3 = 20k)
      Przy 8,4 V: `8,4 × 20/71 = 2,37 V` — zakres liniowy ADC
      (poprzednio 33k dawało 3,18 V, powyżej full-scale).
      W firmware: `BATTERY_DIVIDER_R_TOP_OHM 51000`, `R_BOTTOM_OHM 20000`.

- [x] **R31 na wyjściu 74AHCT1G125** — U3 pad 4 → R31 330R → DIN D1.
      (Wartość zmieniona z 220R na 330R.)

- [x] **Q1 (P-MOSFET, zabezpieczenie polaryzacji) — orientacja poprawiona.**
      Wg netlisty: **D (pad 3) → J1 pin 2 (bateria +)**, **S (pad 2) →
      SW_PWR_1**, G (pad 1) → RS1 100k → GND. Ścieżki na PCB przepięte
      i połączone (każdy z trzech netów to jedna spójna grupa pad–ścieżka–pad).
      Przy poprawnej polaryzacji dioda podłożowa przewodzi, Vgs ≈ −Vbat
      włącza kanał; przy odwrotnej Vgs > 0 i dioda zaporowo — blokuje.
- [ ] **Q1 — wybrać konkretny tranzystor** (Value w schemacie to tylko
      `Q_PMOS_GSD`). Footprint SOT-23 z pinoutem 1=G, 2=S, 3=D — pasuje
      np. AO3401A, DMG2305UX. Wymagania: Vds ≥ 20 V, **|Vgs max| ≥ 12 V**
      (pełny pakiet daje Vgs ≈ −8,4 V, bez zenera G-S — Si2301 z ±8 V
      odpada), Id ≥ 2 A, niski Rds(on) przy Vgs = −6 V.

- [ ] **J1 — złącze baterii.** Obecnie footprint `2pin_header_new`
      (pady na przewód, raster 2,54 mm), **nie** kluczowany JST-XH.
      Pakiet wchodzi luźnym przewodem z zewnątrz obudowy — rozważyć
      kluczowane złącze (Q1 chroni elektronikę, ale klucz chroni przed
      samym błędem).

- [ ] **(opcjonalnie) nRF24 IRQ → GP16** — obecnie niepodłączony.
      **Nie jest potrzebne.** Przy 250 kbps transakcja trwa ~1 ms,
      a pętla ma 20 ms. IRQ ma sens dopiero przy 200+ Hz, przy trybach
      sleep, albo na odbiorniku (jitter serwa). Dołóż tylko, jeśli ścieżka
      wchodzi bez psucia routingu.

**USUNIĘTE z listy:**
- ~~przycisk reset na własnym PCB~~ — moduł ma Key1 (BOOT) i Key2 (RESET).
  Zamiast tego: **zaplanować w obudowie dostęp do obu przycisków modułu.**
  Sam RUN nie wystarczy, bo GPIO0 nie jest na listwie, a bez BOOT-a nie
  wymusisz trybu download, gdy auto-program zawiedzie.
- ~~sprawdzić obciążalność U2~~ — **nieaktualne przy wariancie B.** U2 zasila
  już tylko WS2812B (do 300 mA) wobec 3 A. Zapas 10×.
- ~~zmierzyć napięcie wyjściowe U2~~ — nieistotne, odkąd U2 nie zasila MCU.

- [ ] **OLED — złącze uniwersalne I2C/SPI (decyzja 2026-11-12).**
      OLED nie siedzi na PCB: wpięty krótkimi przewodami w terminal,
      przykręcony do plexi. Footprint modułu OLED znika z PCB, w jego miejsce
      **7-pinowy terminal**, żeby w razie potrzeby przejść na OLED SPI bez
      nowej płytki:

      | pin | I2C (montaż domyślny) | SPI (wariant awaryjny) |
      |---|---|---|
      | 1 | GND | GND |
      | 2 | +3.3V | +3.3V |
      | 3 | GP17 → R7 33R → SCL | SCK |
      | 4 | GP18 → R8 33R → SDA | MOSI |
      | 5 | — | CS ← GP16 |
      | 6 | — | DC ← GP33 |
      | 7 | — | RES ← GP40 |

      - R5/R6 (pull-upy 5k1) oznaczyć w schemacie jako **„tylko I2C”** —
        przy SPI nie lutować. R7/R8 33R zostają w obu wariantach.
      - SPI dla OLED na **osobnym kontrolerze** (SPI3 przez matrycę GPIO),
        nie dzieli magistrali z nRF24 (SPI2).
      - Po zmianie wolne zostaje tylko GP10 — świadomie, wolne piny nie są
        potrzebne (ewentualne zmiany → rewizja V2).
      - **Moduł: Waveshare 1.3inch OLED (B)** (Botland 10451), SH1106 128×64,
        3,3/5 V, 40,5 × 37,5 mm. Fabrycznie **4-wire SPI** (BS0=0, BS1=0);
        I2C wymaga przelutowania rezystorów BS na spodzie (BS1=1, BS0=0),
        wtedy DIN=SDA, CLK=SCL, **D/C = adres** (GND → 0x3C, więc GP33 trzymać
        w stanie niskim), RST nadal używany. Wiki:
        https://www.waveshare.com/wiki/1.3inch_OLED_(B)
      - **Pinout modułu (Botland, goldpin 2,54 mm, 8 pinów):** 1 VCC, 2 GND,
        3 NC, 4 DIN, 5 CLK, 6 CS (aktywny niski), 7 D/C, 8 RES (aktywny niski).
        Terminal na PCB robimy **8-pinowy w tej samej kolejności**, żeby
        przewód szedł 1:1: 1 +3.3V, 2 GND, 3 NC, 4 GP18, 5 GP17, 6 GP16,
        7 GP33, 8 GP40. (Zastępuje tabelę 7-pinową wyżej.)
      - Opcjonalnie pull-up 10k na CS — wyświetlacz odznaczony podczas bootu ESP.
      - **Propozycja (do decyzji): start od razu na SPI** (ustawienie fabryczne, bez
        lutowania; R5/R6 niewlutowane, footprinty jako zapas dla I2C).
        Firmware: konstruktor U8g2 dla SH1106 4W HW SPI + piny w `hardware_config.h`.

### 1B. Porządki w schemacie (miedź bez zmian)

- [x] Etykiety tekstowe w `MCU.kicad_sch`: `SCL=GP17`, `SDA=GP18`
      (poprawione), SPI `CE=GP34, SCK=GP35, MISO=GP36, MOSI=GP37, CSN=GP38`
      zgodne z global labelami.
- [x] Etykiety gimbali w `MCU.kicad_sch` i `Buttons.kicad_sch` zgodne
      z netami (`LY=CH0, LX=CH1, JOY L SW=CH3, RY=CH4, RX=CH5, JOY R SW=CH6`).
- [ ] **Etykiety tekstowe w `Radio.kicad_sch` wyglądają na przesunięte**
      względem global labeli obok nich (np. tekst `MISO` przy `GP35`,
      `SCK` przy `GP34`, `CE` przy `GP38`). Miedź jest poprawna (netlista
      TX1 zgodna z firmware) — to tylko opisy. Sprawdzić wizualnie i poprawić.
- [x] Duplikat referencji `F1` — brak duplikatów w PCB (jest tylko polyfuse
      F1 i przycisk SW_F1).
- [ ] Sprawdzić rating polyfuse (MF-RG300: 3 A hold) względem prądu
      rozruchowego bucków i zakresu napięcia.
- [ ] Ujednolicić opis wyświetlacza (SSD1306 w symbolu vs SH1106 w firmware).
- [ ] Przeliczyć ERC na czysto, bump rev → V2.

### 1C. Layout

- [ ] **`C7 100u` i `C8 100n` są ~20 mm od TX1** (odległość między
      punktami odniesienia footprintów; C10 10n ~13 mm). Docelowo mają
      siedzieć **fizycznie przy pinach VCC/GND nRF24** — najczęstsza
      przyczyna „radio działa na 3 m". Przesunąć bliżej albo potwierdzić
      w 3D, że pady są blisko pinów 1/2 modułu.
- [ ] DRC + wizualna kontrola 3D

### 1D. Do sprawdzenia na zmontowanej płytce (bring-up)

- [ ] **Oscyloskop na VSYS, pierwsze 10 ms po pstryknięciu.**
      Moduł przy rozruchu ładuje ~69 µF (C37 47 µ + CP1 22 µ) przez
      diodę i chwilowo ściągnie VSYS w dół. AP63203 ma soft-start
      i 2 A zapasu, więc powinno być czysto.
      **Próg alarmowy: zapad poniżej ~2,6 V.** Gdyby wystąpił —
      zwiększyć CP1 albo obniżyć Vf (PMEG2010AEH zamiast B140).

- [ ] **Zmierzyć VSYS w normalnej pracy.** Oczekiwane ~2,9 V (B140).
      Poniżej 2,7 V oznacza, że spadek na diodzie jest większy niż
      zakładany — sprawdzić rzeczywisty prąd wejściowy modułu.

- [ ] **Zasięg radia po podniesieniu PA_LEVEL.** Wariant B stawia nRF24
      na wspólnym railu z MCU. Jeśli zasięg odbiega od oczekiwań dla
      PA/LNA, pierwszym podejrzanym jest sprzężenie przez rail 3.3V —
      sprawdzić ripple na VCC radia przy TX.

- [ ] **Pierwsze uruchomienie firmware na PCB:** strona `io_readings` —
      przejść każde wejście (osie, przyciski, LDR, bateria) i porównać
      z tabelą pinów. Potem kalibracja gimbali od nowa.

---

## Założenia rewizji V2 PCB (ustalone 2026-11-12, po V1)

- **Enkoder EC11 z przyciskiem zamiast D-pada** (SW_U/D/L/R/C znikają).
  **Schemat V2 (2026-11-12): ENC_A = GP14, ENC_B = GP15, ENC_SW = GP12.**
  Obwody po dawnym D-padzie: R14/R21 (A), R13/R20 (B), R15/R22 (SW);
  C12/C13 = 10 nF (A/B), C14 = 100 nF (SW).
  A/B: pull-up 10k + 10 nF do GND (filtr RC na drgania styków).
  Firmware: dekodowanie kwadratury sprzętowo (PCNT), podpięcie pod
  zdarzenia `ui_input` (`pageNext`/`selectNext`/`enter`); obrót = zmiana
  wartości/pozycji.
  **Montaż: enkoder w plexi** (tuleja M7 + nakrętka, najkrótszy wałek ~15 mm,
  niskie pokrętło; wybrany PEC11R ma wałek 20 mm), przewodami do **5-pinowego terminala na PCB** w kolejności
  pinów enkodera: 1 A, 2 C (GND), 3 B, 4 S1 (przycisk), 5 S2 (GND).
  Na PCB: pull-up 10k + 10 nF na A i B, pull-up 10k (+ opc. 1k) na S1.
  Symbol: `Device:RotaryEncoder_Switch` (EC11, np. Alps EC11E lub odpowiednik).
- **Przyciski: F1 (link), F2 (arming), F3 (wstecz)** — bez D-pada, bez
  drabinki rezystorowej (decyzja 2026-11-12).
  **Schemat V2 (stan 2026-11-13):** F1 = GP42 (SW_F1, R19/R26, C18),
  F2 = GP41 (SW_F2, R18/R25, C17), F3 = GP11 (symbol nadal nazywa się
  SW_D1, R17/R24, C16 — do przemianowania na SW_F3). Wolne: GP10, GP13.
- **Enkoder wybrany: Bourns PEC11R** (TME), 24 imp./24 zatrzaski,
  z przyciskiem, wałek radełkowany 6 × 20 mm (gałka pod radełko 6 mm albo na
  śrubkę). 1 zatrzask = pełny cykl kwadratury (4 zbocza) → w firmware
  1 krok = 4 zbocza PCNT.
- **Obsługa UI (styl panelu Carel pGD1):** enkoder przewija ekrany/menu,
  klik wchodzi w opcję, obrót zmienia wartość, klik zatwierdza i przechodzi
  do następnej zmiennej; po ostatniej zmiennej obrót znów przewija ekrany.
  F3 = wstecz. Dedykowany F2 do armowania zostaje (bezpieczeństwo).
- **Trymy przez enkoder na ekranie głównym:** obrót = trym wybranej osi,
  klik = następna oś (LX→LY→RX→RY), F3 = wejście do menu. OLED pokazuje
  np. „TRIM RX −3". Firmware: trym per oś w NVS (jak deadzone/limit),
  dodawany do wyjścia po krzywej i limicie, clamp −100..100, zakres ±20, krok 1.
- **ESP32 w centrum PCB** (miejsce po footprincie OLED), blisko nRF24:
  - OK pod warunkiem, że Wi-Fi/BLE ESP32 nie są używane — antena modułu
    w środku płytki i 2,4 GHz kilka cm od nRF. Jeśli BLE kiedyś potrzebne →
    antena modułu przy krawędzi / wycięcie masy.
  - ciągła wylewka GND pod ESP i nRF, przelotki wokół nRF
  - AP63203 i buck 5V z dala od nRF i toru do SMA
  - C7/C8 przy pinach VCC/GND nRF24
  - analogi gimbali (GP1–GP7) z dala od SPI i przetwornic, krótkie
- Złącze OLED (patrz FAZA 1A) przenosi się do V2. **Stan schematu V2
  (2026-11-13):** DIN ← GP18 (bez rezystora), CLK ← R7 33R ← GP17
  (+ R5 5k1 pull-up — przy SPI DNP), CS ← R8 33R ← GP16 (+ R6 5k1 pull-up,
  zostaje). Do dokończenia: etykieta GP16 przy ESP niepodpięta do pinu,
  D/C → GP33 i RES → GP40 niepodłączone, NC (pin 3) bez znacznika no-connect.

---

## FAZA 2 — Obudowa (Fusion 360)

- [x] Eksport PCB z KiCada do **STEP** (`PCB/FlexRC/FlexRC.step`)
- [ ] Import STEP do Fusion jako referencja wymiarowa
- [ ] Model korpusu wokół realnej płytki
- [ ] **Wydruk makiety bez elektroniki** — kontrola ergonomii w dłoni
- [ ] Dopiero po makiecie: ewentualne poprawki PCB
- [ ] Otwory: USB-C modułu, okno OLED, okno LDR (z przegrodą przed
      światłem własnych diod RGB), przepust SMA, przepust przewodu 2S,
      dostęp do Key1 (BOOT) i Key2 (RESET) modułu
- [ ] Mocowanie koszyka 2S pod spodem
- [ ] Rozważyć mosiężny panel przedni — waga: mosiądz ~8,5 g/cm³,
      panel 150×100×2 mm ≈ **255 g**. Korpus w całości z blachy 2 mm
      to 600–900 g, za dużo na trzymany w rękach nadajnik.

---

## FAZA 3 — Firmware

- [x] **`git tag breadboard-working`** — założony na ostatnim commicie
      sprzed zmiany pinów (`45ff894`). Punkt odniesienia „tu wszystko działało".
- [x] **Pełne przemapowanie stykówka → PCB** — `hardware_config.h` wg
      netlisty `FlexRC.kicad_pcb`, każdy sygnał zweryfikowany (patrz
      „Mapa pinów" wyżej). Zmieniły się gimbale, przyciski gimbali, D-pad
      i całe SPI nRF24. Etykiety w `io_readings.cpp` dopasowane.
      Build `pio run -e controller` przechodzi; na sprzęcie nietestowane.
- [x] Dzielnik baterii w `config.h`: 51k / 20k (zgodnie z PCB).
- [ ] Podnieść `NRF_PA_LEVEL` z PA_MIN (nadajnik i odbiornik) — moduł ma
      PA/LNA, bring-up na minimum był świadomy, ale docelowo trzeba więcej.
- [ ] Ujednolicić próg pustej baterii (`BATTERY_CELL_EMPTY_MV 6600` vs 6,4 V).
- [ ] **Timing TX:** flush OLED (~25 ms, page buffer SH1106) blokuje TX
      w tej samej pętli. Kierunek: bufor pełny (`_F_`) + wysyłanie po jednym
      wierszu kafli na obieg pętli (`updateDisplayArea`, ~3 ms), harmonogram
      TX `lastTxMs += TX_TICK_MS` zamiast `= now`. Osobny task FreeRTOS dla
      TX dopiero, jeśli pomiar (GPIO/`micros()` wokół `commSendFrame()`
      i `displayTick()`) po tych zmianach nadal pokaże za duży jitter.
- [ ] Wyłączyć `BATTERY_DEBUG`/`LINK_DEBUG` w `receiver.cpp` poza debugowaniem.

**Skalowanie gimbali (`Joystick::processAxis()` w `joysticks.cpp`)** — przegląd 2026-11-12:

- [ ] **Krzywa expo → `out = 100·((1−k)·n + k·nᵖ)`, parametry k i p per oś w menu.**
      Decyzja 2026-11-12. Obecnie `powf(norm, 1+expo)` (domyślnie 1,8) ma zerowe
      nachylenie w środku: pierwsza jedynka na wyjściu dopiero ~15% skoku za
      deadzone. Nowy wzór zachowuje miękki środek, ale przy k < 1 zostaje mała
      liniowa czułość przy zerze.
      - **k** (0..1) — ile krzywej: 0 = liniowo, 1 = czyste nᵖ.
      - **p** (np. 2..5, krok 0,5) — kształt. Przy k = 1 wzór = stara krzywa
        z `expo = p − 1` (p = 4, k = 1 ≡ stare expo 3), więc p = 4 obejmuje
        cały dotychczasowy zakres.
      - Odpowiedniki przy p = 4 (zgodność w połowie skoku): stare expo 1 →
        k ≈ 0,57, stare 1,8 → k ≈ 0,81. Domyślnie: p = 4, k = 0,57 (punkt startowy, i tak regulowane w menu).
      - Do zmiany: wzór w `processAxis()`; `ExpoData` w NVS rozszerzyć o p
        per oś + nowy `EXPO_MAGIC` (stare wartości się nie wczytają);
        `clampExpo` → 0..1, nowy `clampP`; `JOY_EXPO_DEFAULT` → k, nowy
        `JOY_EXPO_P_DEFAULT`; API `joysticksGet/Set...` dla p;
        strona `set_expo.cpp`: nowa pozycja p dla każdej osi, ten sam wzór
        w podglądzie (`pctFor`) i w `clampExpoLocal`.
      - Wydajność bez znaczenia (ESP32-S3 z FPU, `powf` ~µs × 4 osie × 50 Hz).
      - Weryfikacja: arkusz Excel z symulacją (prompt w
        `prompt-excel-joystick.md`, kolumna „Nowa” z `norm^p`).
- [ ] **Środek kalibracji uśredniać** (~16 próbek) zamiast jednego odczytu
      `readRawInvertedX/Y()` w `calib_joy.cpp`.
- [ ] **Uśrednianie osi** — 4 próbki `analogRead()` na odczyt (szum ADC
      ESP32-S3 ±10–20 jednostek widoczny przy końcach skoku). Pokrywa się
      z punktem „Oversampling ADC" niżej.
- [ ] **Margines na końcach skoku** (~2%) — `calMax/calMin` to szczyt
      zaszumionych próbek, więc 100% bywa nieosiągalne albo miga 97–100.
- [ ] **Limit deadzone** `ADC_MAX*400/1023` w `clampDeadzone()` to relikt
      10-bitowego ADC (pozwala na ~78% połówki skoku) — zmniejszyć.

Stan reszty punktów, zweryfikowany w kodzie:

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
      timeoutu na `lastRxAt`; przy utracie linku wyjścia trzymają
      ostatnią odebraną wartość PWM zamiast się wyzerować.
      **Priorytet: wyższy niż timing TX** (bezpieczeństwo).
- [x] UI na OLED — gotowe: `menu.cpp` + strony ustawień (kalibracja,
      expo, test LED, czułość fotorezystora, podgląd I/O).
- [x] Auto-jasność LED z LDR — gotowe (`photoSensorLedBrightnessPct`,
      używane w `receiver.cpp` i `control_link.cpp`).
- [ ] Odbiornik: wyjścia PWM/PPM/SBUS + **failsafe przy utracie linku** —
      tylko `analogWrite` na 2 kanały (steering z `rx`, throttle z `ly`,
      bez kierunku) w `src/receivers/test_platform/main.cpp`. Brak sygnału
      serwo 50 Hz, PPM/SBUS nie istnieją. Failsafe nie istnieje.
- [ ] **(z `do_zrobienia.txt`)** LED/wskaźnik na odbiorniku sterowany
      wysterowaniem kanałów analogowych — pomysł nowy, jeszcze niezrobiony.

---

## Kolejność

**Nie zamawiaj PCB przed ukończeniem FAZY 1 i punktu „wydruk makiety" z FAZY 2.**
Makieta jest tania i wyłapie większość błędów ergonomii, których nie widać w CAD.

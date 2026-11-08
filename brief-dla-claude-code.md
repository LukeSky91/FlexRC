# Brief: aktualizacja CLAUDE.md

Zaktualizuj `CLAUDE.md` w roocie repo. **Nie zmieniaj żadnego kodu.**

Podział pracy:
- **Ty** opisujesz stan faktyczny z repo — architekturę, mapę pinów w firmware, protokół, środowiska build, TODO.
- **Poniżej** masz gotowe bloki dotyczące sprzętu i decyzji projektowych. Wklej je bez zmian merytorycznych — to ustalenia z osobnej analizy schematu i dokumentacji Waveshare, których nie zweryfikujesz z poziomu repo.

---

## CZĘŚĆ A — co masz napisać sam (z repo)

### A1. Sekcja „Stan projektu" — popraw błąd

Obecny plik mówi „Kod nadajnika wstępny, odbiornik prosty". **To nieprawda.** Napisz stan faktyczny na podstawie repo. Uwzględnij:
- że firmware nadajnika jest modularny i rozbudowany (wymień warstwy)
- pełne UI OLED ze stronami ustawień
- maszynę stanów linku + arming/failsafe
- telemetrię przez ACK payload
- że **kod działa na płytce stykowej** — to jest zweryfikowany, działający punkt odniesienia

### A2. Sekcja „Architektura firmware" — nowa

Opisz z repo:
- środowiska z `platformio.ini` (nadajnik + odbiorniki testowe)
- podział `src/controller/` na moduły
- rolę `lib/common`
- protokół radiowy z `comm.cpp`: rozmiar ramki, zawartość, ACK payload, bitrate, CRC, retries
- aktualny `NRF_PA_LEVEL` i zaznacz, że to ustawienie bring-up

### A3. Sekcja „Mapa pinów" — przebuduj na DWIE kolumny

To najważniejsza zmiana w pliku.

Kontekst: piny zostały **celowo przemapowane** na etapie projektu PCB, żeby ograniczyć krzyżowania ścieżek. Firmware odpowiada stykówce. Schemat odpowiada PCB. **Żadna wersja nie jest błędem** — to dwie rewizje sprzętu.

Zrób tabelę z kolumnami:

| Sygnał | GPIO — firmware (stykówka) | GPIO — schemat (PCB) | Zgodne? |
|---|---|---|---|

Wypełnij ją **czytając `include/controller/hardware_config.h`** i zestawiając z tabelą PCB, którą masz w istniejącym CLAUDE.md. Oznacz każdą rozbieżność.

Wiem o jednej (`nRF24 MOSI/MISO`). **Sprawdź, czy nie ma więcej** — przemapowanie pod routing rzadko dotyczy jednej pary. Przejdź wszystkie sygnały, nie tylko SPI.

### A4. Sekcja TODO

Zaciągnij `do_zrobienia.txt` i połącz z FAZĄ 3 z istniejącego pliku. Oznacz, co już jest zrobione w kodzie.

---

## CZĘŚĆ B — bloki do wklejenia bez zmian

### B1. Zastąp sekcję „Sprzęt → MCU" tym:

```markdown
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
```

### B2. Zastąp opis zasilania tym:

```markdown
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
```

### B3. Wstaw jako nową sekcję, wysoko w pliku:

```markdown
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
```

### B4. Zastąp CAŁĄ sekcję „FAZA 1 → 1A" tym:

```markdown
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
      W firmware: `BATTERY_DIVIDER_R_TOP_OHM` 33000 → 47000.

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
```

### B5. Wstaw przed sekcją „Kolejność":

```markdown
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
```

### B6. Dopisz do FAZY 3 (firmware), na początku:

```markdown
- [ ] **`git tag breadboard-working` PRZED jakąkolwiek zmianą pinów**
      To jedyny punkt odniesienia „tu wszystko działało". Bez niego,
      gdy PCB nie ruszy, nie odróżnisz błędu przemapowania od wady sprzętu.
- [ ] **Pełne przemapowanie stykówka → PCB**, nie punktowa łatka na MOSI/MISO.
      Przejść całą tabelę pinów, sygnał po sygnale.
- [ ] Po przemapowaniu: podnieść `NRF_PA_LEVEL` z PA_MIN — moduł ma PA/LNA,
      bring-up na minimum był świadomy, ale docelowo trzeba więcej.
```

---

## CZĘŚĆ C — zasady

1. **Nie zmieniaj kodu.** Tylko `CLAUDE.md`.
2. Bloki z części B wklej merytorycznie bez zmian. Możesz dostosować
   nagłówki i numerację do struktury pliku.
3. Nie usuwaj FAZY 2 (obudowa) — jest aktualna.
4. Jeśli znajdziesz w repo coś, co przeczy blokom z części B —
   **nie poprawiaj po cichu.** Zgłoś rozbieżność w odpowiedzi.
5. Na końcu wypisz listę wszystkiego, co zmieniłeś w pliku.

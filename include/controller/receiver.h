#pragma once
#include "common/comm.h"

// Inicjalizacja odbiornika (wymaga wczesniejszego commInit)
void receiverInit();

// Wysyla ramke nadawcza i obsluguje odbior; ustawia LED 3RD wg baterii z ramki RX.
// txFrame: lokalne wartosci do wyslania.
void receiverLoop(const CommFrame& txFrame);

// Ostatni odebrany stan baterii odbiornika z ramki RX.
uint16_t receiverGetBatteryPct();

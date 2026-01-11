#pragma once
#include "common/comm.h"

// Inicjalizacja odbiornika (wymaga wczesniejszego commInit)
void receiverInit();

// Wysyla ramke nadawcza i obsluguje odbior; ustawia LED 3RD wg aux z ramki RX.
// txFrame: lokalne wartosci do wyslania.
void receiverLoop(const CommFrame& txFrame);

// Ostatni odebrany AUX (0-100) z ramki RX; zwraca 0, jesli nic nie odebrano.
uint16_t receiverGetLastAux();

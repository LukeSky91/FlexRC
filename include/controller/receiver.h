#pragma once
#include "common/comm.h"

enum class ReceiverLinkState : uint8_t
{
    Idle = 0,
    Connecting,
    Connected,
    Lost,
    RadioError
};

// Inicjalizacja odbiornika (wymaga wczesniejszego commInit)
void receiverInit(bool radioReady);

// Wysyla ramke nadawcza i obsluguje odbior; ustawia LED 3RD wg baterii z ramki RX.
// txFrame: lokalne wartosci do wyslania.
void receiverLoop(const CommFrame& txFrame);

void receiverSetLinkEnabled(bool enabled);
bool receiverIsLinkEnabled();
ReceiverLinkState receiverGetLinkState();
const char *receiverGetLinkStateShortName();

// Ostatni odebrany stan baterii odbiornika z ramki RX.
uint16_t receiverGetBatteryPct();

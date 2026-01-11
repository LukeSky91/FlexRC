#include <Arduino.h>
#include "receivers/test_platform/config.h"
#include "common/comm.h"


static CommFrame lastRx{};
static uint32_t lastLog = 0;
static uint32_t lastHeartbeat = 0;
static bool radioReady = false;
static uint32_t lastRxAt = 0;
static uint32_t rxCount = 0;
static uint32_t lastDiag = 0;

void setup()
{
#if SERIAL_ENABLED
    Serial.begin(SERIAL_BAUD); // USB serial logs
#endif

#if NRF_ENABLED
    radioReady = commInit(NRF24_CE_PIN, NRF24_CSN_PIN, NRF_CHANNEL, NRF_ADDR);
#if SERIAL_ENABLED
    if (!radioReady)
    {
        Serial.println("NRF24 not detected, radio disabled");
    }
#endif
#endif
    pinMode(AUX_PIN, INPUT);

#if SERIAL_ENABLED
    Serial.println("Receiver (Nano) start");
#endif
}

void loop()
{
#if SERIAL_ENABLED
    // Heartbeat to confirm loop is running
    if (millis() - lastHeartbeat >= 1000)
    {
        lastHeartbeat = millis();
        Serial.println("tick");
    }

    // Radio diagnostics (1 Hz)
    if (millis() - lastDiag >= 1000)
    {
        lastDiag = millis();
        Serial.print("RADIO: ");
        Serial.print(radioReady ? "OK" : "OFF");
        Serial.print(" | RX/s: ");
        Serial.print(rxCount);
        Serial.print(" | lastRxAge ms: ");
        Serial.println(millis() - lastRxAt);
        rxCount = 0;
    }
#endif

    // Send AUX (pot/battery) to transmitter
    CommFrame tx{};
    uint16_t rawAux = (uint16_t)analogRead(AUX_PIN);
    tx.aux = (uint8_t)((rawAux * 100UL + 511) / 1023);

// Receive control frame
#if NRF_ENABLED
    if (radioReady)
    {
        commSendFrame(tx);
    }
    CommFrame rx{};
    if (radioReady && commPollFrame(rx))
    {
        lastRx = rx;
        lastRxAt = millis();
        rxCount++;
    }
#endif

    // Log every 250 ms
    if (millis() - lastLog >= 250)
    {
        lastLog = millis();
#if SERIAL_ENABLED
        Serial.print(" | LX: ");
        Serial.print(lastRx.lx);
        Serial.print(" | LY: ");
        Serial.print(lastRx.ly);
        Serial.print(" | RX: ");
        Serial.print(lastRx.rx);
        Serial.print(" | RY: ");
        Serial.print(lastRx.ry);
        Serial.print(" | AUX OUT: ");
        Serial.print(tx.aux);
        Serial.println();
#endif
    }
}

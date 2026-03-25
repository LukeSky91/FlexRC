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
static uint16_t lastBatteryMv = 0;
static uint8_t lastBatteryPct = 0;

static uint8_t batteryPctFromMv(uint32_t mv)
{
    if (mv <= BATTERY_CELL_EMPTY_MV)
        return 0;
    if (mv >= BATTERY_CELL_FULL_MV)
        return 100;

    const uint32_t span = (uint32_t)BATTERY_CELL_FULL_MV - (uint32_t)BATTERY_CELL_EMPTY_MV;
    return (uint8_t)(((mv - BATTERY_CELL_EMPTY_MV) * 100UL + (span / 2UL)) / span);
}

static uint16_t readBatteryMv()
{
    uint32_t rawSum = 0;
    for (uint8_t i = 0; i < BATTERY_AVG_SAMPLES; ++i)
    {
        rawSum += (uint32_t)analogRead(BATTERY_PIN);
    }

    const uint32_t raw = rawSum / BATTERY_AVG_SAMPLES;
    const uint32_t adcMv = (raw * BATTERY_ADC_REF_MV + 511UL) / 1023UL;
    const uint32_t battMv =
        (adcMv * (BATTERY_DIVIDER_R_TOP_OHM + BATTERY_DIVIDER_R_BOTTOM_OHM) + (BATTERY_DIVIDER_R_BOTTOM_OHM / 2UL)) /
        BATTERY_DIVIDER_R_BOTTOM_OHM;

    return (battMv > 0xFFFFUL) ? 0xFFFFU : (uint16_t)battMv;
}

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
    pinMode(BATTERY_PIN, INPUT);

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

    static uint32_t lastBatteryRead = 0;
    if (millis() - lastBatteryRead >= BATTERY_READ_INTERVAL_MS)
    {
        lastBatteryRead = millis();
        lastBatteryMv = readBatteryMv();
        lastBatteryPct = batteryPctFromMv(lastBatteryMv);
    }

    // Send battery telemetry to transmitter
    CommFrame tx{};
    tx.battPct = lastBatteryPct;

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
        Serial.print(" | JL: ");
        Serial.print((lastRx.joyButtons & 0x01u) ? 1 : 0);
        Serial.print(" | JR: ");
        Serial.print((lastRx.joyButtons & 0x02u) ? 1 : 0);
        Serial.print(" | BATT: ");
        Serial.print(tx.battPct);
        Serial.print("%");
        Serial.println();
#endif
    }
}

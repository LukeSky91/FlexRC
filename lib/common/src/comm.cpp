#if defined(RX_VARIANT_TEST_PLATFORM)
#include "receivers/test_platform/config.h"
#else
#include "controller/config.h"
#endif

#include <common/comm.h>

#include <RF24.h>
#include <string.h>

/*
 * Global RF24 instance.
 * Allocated once after pins are known.
 */
static RF24 *gRadio = nullptr;
static bool gRadioOk = false;

/*
 * Shared 5-byte address used for TX and RX pipes.
 */
static uint8_t gAddr[5] = {0};

/*
 * On-air packet formats (packed).
 * These are NOT exposed outside this file.
 */
#pragma pack(push, 1)
struct TxPkt
{
    int8_t lx;
    int8_t ly;
    int8_t rx;
    int8_t ry;
};

struct AckPkt
{
    uint8_t aux;   // 0..100 telemetry value
    uint8_t flags; // reserved for future use
};
#pragma pack(pop)

static_assert(sizeof(TxPkt) == 4, "TxPkt size must be exactly 4 bytes");
static_assert(sizeof(AckPkt) == 2, "AckPkt size must be exactly 2 bytes");

#ifdef ROLE_RECEIVER
// Cached ACK payload sent back to controller
static AckPkt gAck = {0, 0};
#endif

bool commInit(uint8_t cePin,
              uint8_t csnPin,
              uint8_t channel,
              const uint8_t address[5])
{
    memcpy(gAddr, address, 5);

    // Allocate RF24 object once
    if (!gRadio)
        gRadio = new RF24(cePin, csnPin);

    if (!gRadio->begin())
    {
        gRadioOk = false;
        return false;
    }

    // Extra safety check (useful during bring-up)
    if (!gRadio->isChipConnected())
    {
        gRadioOk = false;
        return false;
    }

    // Stable, short-range configuration
    gRadio->setChannel(channel);
    gRadio->setDataRate(RF24_250KBPS);   // most robust
    gRadio->setPALevel(NRF_PA_LEVEL);
    gRadio->setCRCLength(RF24_CRC_16);   // strong CRC
    gRadio->setRetries(3, 5);            // delay, count
    gRadio->setAutoAck(true);
    gRadio->enableAckPayload();          // enable telemetry via ACK
    gRadio->setPayloadSize(sizeof(TxPkt));

    /*
     * Pipe usage:
     * - Writing pipe: used by controller to send control packets
     * - Reading pipe 1: used by receiver to receive control packets
     *   and to attach ACK payloads
     */
    gRadio->openWritingPipe(gAddr);
    gRadio->openReadingPipe(1, gAddr);

    // Start in listening mode (safe default)
    gRadio->startListening();

    gRadioOk = true;
    return true;
}

#ifdef ROLE_CONTROLLER

bool commSendFrame(const CommFrame &tx, CommFrame *rxAck)
{
    if (!gRadio || !gRadioOk)
        return false;

    // Convert application frame to on-air packet
    TxPkt pkt{tx.lx, tx.ly, tx.rx, tx.ry};

    // TX requires radio to stop listening
    gRadio->stopListening();
    bool ok = gRadio->write(&pkt, sizeof(pkt));
    gRadio->startListening();

    // Read ACK payload (telemetry) if available
    if (ok && rxAck && gRadio->isAckPayloadAvailable())
    {
        AckPkt ap{};
        gRadio->read(&ap, sizeof(ap));
        rxAck->aux = ap.aux;
    }

    return ok;
}

#elif defined(ROLE_RECEIVER)

bool commSendFrame(const CommFrame &txTelemetry)
{
    if (!gRadio || !gRadioOk)
        return false;

    // Prepare ACK payload (telemetry)
    gAck.aux = txTelemetry.aux;
    gAck.flags = 0;

    // Attach ACK payload to pipe 1 (control RX pipe)
    return gRadio->writeAckPayload(1, &gAck, sizeof(gAck));
}

bool commPollFrame(CommFrame &outFrame)
{
    if (!gRadio || !gRadioOk)
        return false;

    bool got = false;

    // Drain RX FIFO, keep the latest frame
    while (gRadio->available())
    {
        TxPkt pkt{};
        gRadio->read(&pkt, sizeof(pkt));

        outFrame.lx = pkt.lx;
        outFrame.ly = pkt.ly;
        outFrame.rx = pkt.rx;
        outFrame.ry = pkt.ry;

        got = true;
    }

    return got;
}

#endif

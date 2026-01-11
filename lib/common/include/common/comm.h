#pragma once
// #include "ide_compat.h"
#include <Arduino.h>
#include <stdint.h>


/*
 * ===== Application-level communication frame =====
 *
 * This structure represents normalized control and telemetry data
 * used by the application on both controller and receiver side.
 *
 * Note:
 * - Control values are sent from controller to receiver.
 * - Telemetry (aux) is sent back from receiver to controller
 *   via NRF24 ACK payload.
 */
struct CommFrame
{
    int8_t lx;   // -100..100  Left stick X
    int8_t ly;   // -100..100  Left stick Y
    int8_t rx;   // -100..100  Right stick X
    int8_t ry;   // -100..100  Right stick Y

    uint8_t aux; // 0..100  Telemetry value (battery / potentiometer / etc.)
};

/*
 * ===== Radio initialization =====
 *
 * cePin / csnPin : NRF24 control pins
 * channel        : RF channel (0..125, e.g. 76)
 * address        : 5-byte pipe address (must match on TX and RX)
 *
 * Returns true on successful radio initialization.
 */
bool commInit(uint8_t cePin,
              uint8_t csnPin,
              uint8_t channel,
              const uint8_t address[5]);

/*
 * ===== Controller-side API (TX) =====
 */
#ifdef ROLE_CONTROLLER

/*
 * Sends control frame to receiver.
 *
 * If rxAck is not nullptr, telemetry received via ACK payload
 * is written back into rxAck->aux.
 *
 * Returns true if the packet was acknowledged by receiver.
 */
bool commSendFrame(const CommFrame &tx, CommFrame *rxAck /* may be nullptr */);

#endif // ROLE_CONTROLLER

/*
 * ===== Receiver-side API (RX) =====
 */
#ifdef ROLE_RECEIVER

/*
 * Updates ACK payload with telemetry data (aux).
 *
 * This payload will be attached to the next received control packet
 * and sent back automatically to the controller.
 *
 * Returns true if ACK payload was successfully queued.
 */
bool commSendFrame(const CommFrame &tx);

/*
 * Polls for incoming control frames from controller.
 *
 * outFrame is filled with the most recent received packet.
 *
 * Returns true if at least one new packet was read.
 */
bool commPollFrame(CommFrame &outFrame);

#endif // ROLE_RECEIVER

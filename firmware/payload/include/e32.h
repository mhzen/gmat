#pragma once
#include <Arduino.h>
#include "config.h"
#include "payload_control.h"
#include "radio_frame.h"

// TODO: Verify persisted E32 settings against the actual module revision; no configuration writes yet.
// TODO: Validate modem-disconnect detection; floating/stuck-HIGH AUX cannot establish module presence.
struct E32 {
    bool enabled = false, busy = false, settling = true;
    uint32_t started = 0, highAt = 0, lowAt = 0, waitMs = 100;
    bool high = false, auxReady = false;
    RadioFrame frame;
    void begin() {
        if (!config::radioEnabled) return;
        digitalWrite(config::radioM0, LOW); digitalWrite(config::radioM1, LOW);
        pinMode(config::radioM0, OUTPUT); pinMode(config::radioM1, OUTPUT);
        pinMode(config::radioAux, INPUT); // GPIO34 has no internal pullup.
        Serial2.begin(config::radioBaud, SERIAL_8N1, config::radioRx, config::radioTx);
        enabled = true;
        started = millis(); lowAt = started;
    }
    bool ready() const { return enabled && !busy && !settling && auxReady; }
    void stop() {
        if (enabled) Serial2.end();
        enabled = false; busy = false;
    }
    bool poll(uint32_t now) {
        if (!enabled) return true;
        if (digitalRead(config::radioAux) == LOW) {
            if (high) lowAt = now;
            high = false;
        }
        else if (!high) { high = true; highAt = now; }
        auxReady = high && elapsed(now, highAt, 2);
        if (settling || busy) {
            // Allow UART serialization and AUX response before accepting high as completion.
            if (auxReady && elapsed(now, started, waitMs)) {
                settling = false; busy = false;
            } else if (elapsed(now, started, config::txTimeoutMs)) return false;
        } else if (!high && elapsed(now, lowAt, config::txTimeoutMs)) return false;
        return true;
    }
    bool send(char type, const void* bytes, size_t size) {
        if (!ready() || digitalRead(config::radioAux) == LOW) return false;
        uint8_t out[1 + sizeof(Telemetry) + 2];
        size_t count = encodeFrame(type, bytes, size, out);
        if (!count || Serial2.availableForWrite() < int(count)) return false;
        if (Serial2.write(out, count) != count) return false;
        waitMs = (uint32_t(count) * 10000 + config::radioBaud - 1) / config::radioBaud + 5;
        started = millis(); busy = true;
        return true;
    }
};

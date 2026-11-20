#include <Arduino.h>
#include "payload_control.h"
#include "e32.h"
#include "config.h"
#include "bridge.h"

E32 radio;
constexpr int statusLed = 13;
constexpr uint32_t telemetryTimeoutMs = 1000;
bool armedReported = false;
uint32_t lastTelemetry = 0;

void fault(const char* reason) {
    armedReported = false;
    digitalWrite(statusLed, LOW);
    Serial.printf("# FAULT: %s; reset required\n", reason);
    radio.stop();
}
void pollRadio() {
    if (!radio.poll(millis())) { fault("E32 AUX timeout"); return; }
    if (!radio.enabled) return;
    for (unsigned i = 0; i < 64 && Serial2.available(); ++i) {
        Telemetry p;
        if (radio.frame.push(uint8_t(Serial2.read()), 'T', &p, sizeof(p))) {
            if (validTelemetry(p, sizeof(p))) {
                lastTelemetry = millis();
                armedReported = p.state == uint8_t(State::ARMED) && (p.flags & 1u);
                char line[128];
                csv(line, sizeof(line), p);
                Serial.print(line);
            } else Serial.println("# malformed telemetry");
        }
    }
}
void setup() {
    digitalWrite(statusLed, LOW);
    pinMode(statusLed, OUTPUT);
    Serial.begin(115200);
    radio.begin();
    if (!config::radioEnabled) Serial.println("# E32 radio disabled; confirm GCS wiring before enabling");
}
void loop() {
    pollRadio();
    if (armedReported && elapsed(millis(), lastTelemetry, telemetryTimeoutMs)) armedReported = false;
    digitalWrite(statusLed, armedReported ? HIGH : LOW);
}

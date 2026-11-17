#include <Arduino.h>
#include "e32.h"
#include "config.h"
#include "bridge.h"

E32 radio;
SerialCommands commands;

void fault(const char* reason) {
    Serial.printf("# FAULT: %s; reset required\n", reason);
    radio.stop();
}
void pollRadio() {
    const bool wasBusy = radio.busy;
    if (!radio.poll(millis())) { fault("E32 AUX timeout"); return; }
    if (!radio.enabled) return;
    if (wasBusy && !radio.busy) Serial.println("# E32 ready; payload receipt unknown");
    for (unsigned i = 0; i < 64 && Serial2.available(); ++i) {
        Telemetry p;
        if (radio.frame.push(char(Serial2.read()), 'T', &p, sizeof(p))) {
            if (validTelemetry(p, sizeof(p))) {
                char line[128];
                csv(line, sizeof(line), p);
                Serial.print(line);
            } else Serial.println("# malformed telemetry");
        }
    }
}
void setup() {
    // TODO: Confirm GCS board and UART/M0/M1/AUX wiring separately; shared pins come from the payload schematic.
    Serial.begin(115200);
    Serial.println("# BENCH ONLY: no authenticated commands or delivery guarantee");
    radio.begin();
    if (!config::radioEnabled) Serial.println("# E32 radio disabled; confirm GCS wiring before enabling");
}
void loop() {
    pollRadio();
    for (unsigned i = 0; i < 32 && Serial.available(); ++i) {
        Command command;
        bool complete;
        if (!commands.push(char(Serial.read()), command, complete)) {
            if (complete) Serial.println("# malformed serial command");
            continue;
        }
        if (!radio.ready()) { Serial.println("# command rejected: radio unavailable or busy"); continue; }
        CommandPacket p = {uint8_t(command)};
        if (!radio.send('C', &p, sizeof(p))) { fault("E32 UART write failed"); continue; }
        Serial.println("# TX started; payload receipt unknown");
    }
}

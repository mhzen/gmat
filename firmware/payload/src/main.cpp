#include <Arduino.h>
#include <Wire.h>
#include "e32.h"
#include "config.h"
#include "bench.h"
#include "barometer.h"

BenchState bench;
SerialCommands serialCommands;
bool imuHealthy = false, barometerHealthy = false;
BarometerCalibration barometer;
E32 radio;
uint32_t lastTelemetry = 0, sequence = 0;

void fault(const char* reason) {
    bench.fail();
    Serial.printf("# FAULT: %s; reset required\n", reason);
    radio.stop();
}

bool readRegisters(uint8_t address, uint8_t* bytes, uint8_t count, uint8_t device = config::imuAddress) {
    Wire.beginTransmission(device);
    if (Wire.write(address) != 1 || Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(device, size_t(count), true) != count) return false;
    for (uint8_t i = 0; i < count; ++i) {
        int value = Wire.read();
        if (value < 0) return false;
        bytes[i] = uint8_t(value);
    }
    return true;
}

bool writeRegister(uint8_t address, uint8_t value, uint8_t device = config::imuAddress) {
    Wire.beginTransmission(device);
    if (Wire.write(address) != 1 || Wire.write(value) != 1) return false;
    return Wire.endTransmission() == 0;
}

bool configureRegister(uint8_t address, uint8_t value, uint8_t device = config::imuAddress) {
    uint8_t actual;
    return writeRegister(address, value, device) && readRegisters(address, &actual, 1, device) && actual == value;
}

bool startImu() {
    uint8_t identity;
    if (!readRegisters(0x75, &identity, 1) || identity != 0x68) return false;
    if (!writeRegister(0x6B, 0x80)) return false;
    delay(100); // Startup reset settling only; no delays in the main loop.
    if (!configureRegister(0x6B, 0x01) || !configureRegister(0x6C, 0x00) ||
        !configureRegister(0x1A, 0x03) || !configureRegister(0x19, 9) ||
        !configureRegister(0x1B, 0x18) || !configureRegister(0x1C, 0x18)) return false;
    delay(100); // Let the PLL and filter settle before accepting commands.
    return true;
}

bool startBarometer() {
    uint8_t id, status, calibration[24];
    const uint8_t device = config::barometerAddress;
    if (!readRegisters(0xD0, &id, 1, device) || id != 0x58 || !writeRegister(0xE0, 0xB6, device)) return false;
    delay(10); // Startup only: allow reset and NVM copy.
    if (!readRegisters(0xF3, &status, 1, device) || (status & 1) ||
        !readRegisters(0x88, calibration, sizeof(calibration), device) || !barometer.load(calibration)) return false;
    // Normal mode, temperature/pressure x1, 0.5 ms standby, filter coefficient 4.
    return configureRegister(0xF5, 0x08, device) && configureRegister(0xF4, 0x27, device);
}

bool readBarometer(uint32_t& pressure, int16_t& temperature) {
    uint8_t id, control, raw[6];
    const uint8_t device = config::barometerAddress;
    if (!readRegisters(0xD0, &id, 1, device) || id != 0x58 ||
        !readRegisters(0xF4, &control, 1, device) || control != 0x27 ||
        !readRegisters(0xF7, raw, sizeof(raw), device)) return false;
    const uint32_t rawP = (uint32_t(raw[0]) << 12) | (uint32_t(raw[1]) << 4) | (raw[2] >> 4);
    const uint32_t rawT = (uint32_t(raw[3]) << 12) | (uint32_t(raw[4]) << 4) | (raw[5] >> 4);
    return barometer.compensate(rawP, rawT, pressure, temperature);
}

void commandReceived(Command command) {
    Serial.println(bench.apply(command, millis()) ? "# command accepted (simulation only)" : "# command rejected");
}

void pollSerial() {
    // Bound work per loop so continuous serial traffic cannot starve sensor/radio checks.
    for (unsigned i = 0; i < 32 && Serial.available(); ++i) {
        Command command;
        bool complete;
        if (serialCommands.push(char(Serial.read()), command, complete)) commandReceived(command);
        else if (complete) Serial.println("# malformed serial command");
    }
}

void pollRadio(uint32_t now) {
    if (!radio.poll(now)) { fault("E32 AUX timeout"); return; }
    if (!radio.enabled) return;
    for (unsigned i = 0; i < 64 && Serial2.available(); ++i) {
        CommandPacket packet;
        if (radio.frame.push(char(Serial2.read()), 'C', &packet, sizeof(packet))) {
            Command command;
            if (radioCommand(&packet.cmd, sizeof(packet), command)) commandReceived(command);
            else Serial.println("# malformed radio command");
        }
    }
}

void telemetry(uint32_t now) {
    uint8_t raw[14];
    if (!readRegisters(0x3B, raw, sizeof(raw))) {
        imuHealthy = false;
        fault("IMU read failed/short");
        return;
    }
    uint32_t pressure;
    int16_t temperature;
    if (!readBarometer(pressure, temperature)) {
        barometerHealthy = false;
        fault("BMP280 read/configuration/range failure");
        return;
    }
    // TODO: Calibrate on hardware. Preserve sensor axes; ask mounting orientation before directional flight logic.
    // TODO: Flight thresholds, altitude estimation and deployment timing need separate validation.
    Telemetry packet = {sequence++, now,
        accelerationMg(signedWord(raw)), accelerationMg(signedWord(raw + 2)),
        accelerationMg(signedWord(raw + 4)), gyroTenths(signedWord(raw + 8)),
        gyroTenths(signedWord(raw + 10)), gyroTenths(signedWord(raw + 12)),
        uint8_t(bench.state), bench.flags(), pressure, temperature}; // Temperature bytes 6..7 are discarded.
    Serial.printf("%lu,%lu,%d,%d,%d,%d,%d,%d,%u,%u,%lu,%d\n",
        (unsigned long)packet.seq, (unsigned long)packet.t_ms,
        packet.ax, packet.ay, packet.az, packet.gx, packet.gy, packet.gz, packet.state, packet.flags, (unsigned long)packet.pressure_pa, packet.temperature_centi_c);
    bench.reported();
    if (radio.ready() && !radio.send('T', &packet, sizeof(packet))) fault("E32 UART write failed");
    // Busy modem skips this RF sample; fresh USB telemetry continues without a queue.
}

void setup() {
    // TODO: Verify driver polarity, hardware interlocks and boot/reset/brownout safety before any firing implementation.
    digitalWrite(config::pyroControl, LOW);
    pinMode(config::pyroControl, OUTPUT); // Safe-low only; physical firing is not implemented.
    digitalWrite(config::sdCs, HIGH);
    pinMode(config::sdCs, OUTPUT); // Deselect unused SD card.
    // TODO: Implement SD logging with explicit write/full-card errors and bounded impact on sensor/radio timing.
    // TODO: Calibrate GPIO35 ADC with the 33k/10k divider; confirm battery range before voltage reporting or limits.
    Serial.begin(115200);
    Serial.println("# BENCH ONLY: simulated EJECT; no firing output");
    if (Wire.begin(config::sda, config::scl, 100000)) {
        Wire.setTimeOut(20);
        imuHealthy = startImu();
        barometerHealthy = startBarometer();
    }
    if (!barometerHealthy) fault("BMP280 initialization failed");
    if (!imuHealthy) fault("IMU initialization failed");
    if (!bench.fault) radio.begin();
    if (!config::radioEnabled) Serial.println("# E32 radio disabled");
    lastTelemetry = millis();
}

void loop() {
    uint32_t now = millis();
    bench.tick(now, config::telemetryMs);
    pollSerial();
    pollRadio(millis());
    now = millis();
    if (imuHealthy && barometerHealthy && elapsed(now, lastTelemetry, config::telemetryMs)) {
        lastTelemetry = now;
        telemetry(now);
    }
}

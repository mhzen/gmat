#include <Arduino.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <atomic>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "e32.h"
#include "config.h"
#include "payload_control.h"
#include "barometer.h"

PayloadState payloadState;
bool imuHealthy = false, barometerHealthy = false;
BarometerCalibration barometer;
E32 radio;
uint32_t lastTelemetry = 0, sequence = 0;

namespace {
constexpr char logPath[] = "/sd/flight_log.csv";
constexpr char logHeader[] = "seq,t_ms,ax,ay,az,gx,gy,gz,state,flags,pressure_pa,temperature_centi_c\n";
struct LogRow {
    char bytes[128];
    size_t length;
};
QueueHandle_t logQueue = nullptr;
std::atomic<bool> logFailed{false};
bool logOverflowReported = false;

bool writeLog(FILE* file, const char* bytes, size_t length) {
    return fwrite(bytes, 1, length, file) == length &&
        fflush(file) == 0 && fsync(fileno(file)) == 0;
}

void logWriter(void* argument) {
    FILE* file = static_cast<FILE*>(argument);
    LogRow row;
    for (;;) {
        if (xQueueReceive(logQueue, &row, portMAX_DELAY) == pdTRUE &&
            !writeLog(file, row.bytes, row.length)) break;
    }
    logFailed.store(true);
    Serial.println("# SD log write failed; logging disabled until reset");
    fclose(file);
    SD.end();
    vTaskDelete(nullptr);
}

const char* startLog() {
    SPI.begin(config::sck, config::miso, config::mosi, config::sdCs);
    if (!SD.begin(config::sdCs)) return "card mount failed";
    if (unlink(logPath) != 0 && errno != ENOENT) return "old log removal failed";
    FILE* file = fopen(logPath, "w");
    if (!file) return "log creation failed";
    if (!writeLog(file, logHeader, sizeof(logHeader) - 1)) {
        fclose(file);
        return "header write failed";
    }
    logQueue = xQueueCreate(8, sizeof(LogRow));
    if (logQueue && xTaskCreatePinnedToCore(logWriter, "sdLog", 4096, file, 1, nullptr,
                                          1 - xPortGetCoreID()) == pdPASS) return nullptr;
    if (logQueue) vQueueDelete(logQueue);
    logQueue = nullptr;
    fclose(file);
    return "writer startup failed";
}
}

void fault(const char* reason) {
    payloadState.fail();
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
    Serial.println(payloadState.apply(command, millis()) ? "# command accepted" : "# command rejected");
}

void pollRadio(uint32_t now) {
    if (!radio.poll(now)) { fault("E32 AUX timeout"); return; }
    if (!radio.enabled) return;
    for (unsigned i = 0; i < 64 && Serial2.available(); ++i) {
        CommandPacket packet;
        if (radio.frame.push(uint8_t(Serial2.read()), 'C', &packet, sizeof(packet))) {
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
    const Telemetry packet = {sequence++, now,
        accelerationMg(signedWord(raw)), accelerationMg(signedWord(raw + 2)),
        accelerationMg(signedWord(raw + 4)), gyroTenths(signedWord(raw + 8)),
        gyroTenths(signedWord(raw + 10)), gyroTenths(signedWord(raw + 12)),
        uint8_t(payloadState.state), payloadState.flags(), pressure, temperature}; // Temperature bytes 6..7 are discarded.
    LogRow row{};
    const int length = snprintf(row.bytes, sizeof(row.bytes), "%lu,%lu,%d,%d,%d,%d,%d,%d,%u,%u,%lu,%d\n",
        (unsigned long)packet.seq, (unsigned long)packet.t_ms,
        packet.ax, packet.ay, packet.az, packet.gx, packet.gy, packet.gz,
        unsigned(packet.state), unsigned(packet.flags), (unsigned long)packet.pressure_pa, packet.temperature_centi_c);
    if (length > 0 && size_t(length) < sizeof(row.bytes)) {
        row.length = size_t(length);
        Serial.write(reinterpret_cast<const uint8_t*>(row.bytes), row.length);
        if (logQueue && !logFailed.load() && xQueueSend(logQueue, &row, 0) != pdTRUE &&
            !logOverflowReported) {
            logOverflowReported = true;
            Serial.println("# SD log queue full; telemetry rows dropped");
        }
    } else {
        Serial.println("# telemetry CSV formatting failed");
    }
    payloadState.reported();
    if (radio.ready() && !radio.send('T', &packet, sizeof(packet))) fault("E32 UART write failed");
    // Busy modem skips this RF sample; fresh USB telemetry continues without a queue.
}

void setup() {
    // TODO: Hardware-verify driver stage, interlocks and reset/brownout behaviour; GPIO25 floats until setup() runs.
    digitalWrite(config::pyroControl, LOW);
    pinMode(config::pyroControl, OUTPUT); // Safe-low at boot; HIGH only while the FIRE pulse runs.
    digitalWrite(config::sdCs, HIGH);
    pinMode(config::sdCs, OUTPUT); // Deselect the SD card until SPI starts.
    Serial.begin(115200);
    Serial.printf("# WARNING: live pyro output on GPIO25 for %lums per FIRE; keep igniter disconnected during hardware tests\n",
        (unsigned long)config::firePulseMs);
    if (const char* error = startLog()) {
        Serial.printf("# SD %s; logging disabled until reset\n", error);
        SD.end();
    }
    if (Wire.begin(config::sda, config::scl, 100000)) {
        Wire.setTimeOut(20);
        imuHealthy = startImu();
        barometerHealthy = startBarometer();
    }
    if (!barometerHealthy) fault("BMP280 initialization failed");
    if (!imuHealthy) fault("IMU initialization failed");
    if (!payloadState.fault) radio.begin();
    if (!config::radioEnabled) Serial.println("# E32 radio disabled");
    lastTelemetry = millis();
}

void loop() {
    uint32_t now = millis();
    payloadState.tick(now, config::telemetryMs);
    digitalWrite(config::pyroControl, payloadState.fireOn(now, config::firePulseMs) ? HIGH : LOW);
    pollRadio(millis());
    now = millis();
    if (imuHealthy && barometerHealthy && elapsed(now, lastTelemetry, config::telemetryMs)) {
        lastTelemetry = now;
        telemetry(now);
    }
}

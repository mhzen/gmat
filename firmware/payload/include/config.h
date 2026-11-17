#pragma once
#include <stdint.h>

namespace config {
// Payload wiring from schematic/skematik.pdf. GCS wiring must be confirmed separately.
constexpr int sda = 21, scl = 22;
constexpr int sck = 18, miso = 19, mosi = 23;
constexpr int radioRx = 16, radioTx = 17;
constexpr int radioM0 = 32, radioM1 = 33, radioAux = 34;
constexpr int sdCs = 5, batterySense = 35, pyroControl = 25;
constexpr uint8_t barometerAddress = 0x76; // BMP280 SDO grounded.
// TODO: Confirm supply capacity, antenna, persisted UART/RF settings and RF permission before enabling.
constexpr bool radioEnabled = false;
constexpr uint32_t radioBaud = 9600; // Must match the actual E32 configuration, 8N1.
constexpr uint8_t imuAddress = 0x68;  // AD0 low.
constexpr uint32_t telemetryMs = 200;
// TODO: Validate AUX/startup/TX timing and watchdog on hardware at the chosen air rate.
constexpr uint32_t txTimeoutMs = 2000; // Bench watchdog only.
}

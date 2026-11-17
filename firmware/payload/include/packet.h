#pragma once
#include <stddef.h>
#include <stdint.h>

enum class State : uint8_t { IDLE = 0, ARMED, BOOST, COAST, EJECT, DESCENT, LANDED };
enum class Command : uint8_t { ARM = 1, DISARM, FIRE };

#pragma pack(push, 1)
struct Telemetry {
    uint32_t seq, t_ms;
    int16_t ax, ay, az, gx, gy, gz;
    uint8_t state, flags;
    uint32_t pressure_pa;
    int16_t temperature_centi_c;
};
struct CommandPacket { uint8_t cmd; };
#pragma pack(pop)

static_assert(sizeof(Telemetry) == 28, "Telemetry wire size changed");
static_assert(offsetof(Telemetry, pressure_pa) == 22 && offsetof(Telemetry, temperature_centi_c) == 26, "Barometer offsets changed");
static_assert(offsetof(Telemetry, t_ms) == 4 && offsetof(Telemetry, ax) == 8 &&
              offsetof(Telemetry, ay) == 10 && offsetof(Telemetry, az) == 12 &&
              offsetof(Telemetry, gx) == 14 && offsetof(Telemetry, gy) == 16 &&
              offsetof(Telemetry, gz) == 18 && offsetof(Telemetry, state) == 20 &&
              offsetof(Telemetry, flags) == 21, "Telemetry wire offsets changed");
static_assert(sizeof(CommandPacket) == 1, "Command wire size changed");

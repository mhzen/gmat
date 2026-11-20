#pragma once
#include <stdio.h>
#include "packet.h"

inline bool validTelemetry(const Telemetry& p, size_t size) {
    return size == sizeof(p) && p.state <= uint8_t(State::DESCENT) && (p.flags & ~3u) == 0 &&
           p.pressure_pa >= 30000 && p.pressure_pa <= 110000 &&
           p.temperature_centi_c >= -4000 && p.temperature_centi_c <= 8500;
}
inline int csv(char* out, size_t size, const Telemetry& p) {
    return snprintf(out, size, "%lu,%lu,%d,%d,%d,%d,%d,%d,%u,%u,%lu,%d\n",
        (unsigned long)p.seq, (unsigned long)p.t_ms, p.ax, p.ay, p.az,
        p.gx, p.gy, p.gz, p.state, p.flags, (unsigned long)p.pressure_pa, p.temperature_centi_c);
}

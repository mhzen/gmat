#pragma once
#include <math.h>
#include <stdint.h>

inline uint16_t littleWord(const uint8_t* p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
inline int16_t littleSigned(const uint8_t* p) {
    uint16_t v = littleWord(p);
    return int16_t(v < 32768 ? int32_t(v) : int32_t(v) - 65536);
}
struct BarometerCalibration {
    uint16_t t1 = 0, p1 = 0;
    int16_t t2 = 0, t3 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0, p7 = 0, p8 = 0, p9 = 0;
    bool load(const uint8_t* b) {
        t1 = littleWord(b); t2 = littleSigned(b+2); t3 = littleSigned(b+4);
        p1 = littleWord(b+6); p2 = littleSigned(b+8); p3 = littleSigned(b+10);
        p4 = littleSigned(b+12); p5 = littleSigned(b+14); p6 = littleSigned(b+16);
        p7 = littleSigned(b+18); p8 = littleSigned(b+20); p9 = littleSigned(b+22);
        return t1 != 0 && t1 != 0xffff && p1 != 0 && p1 != 0xffff;
    }
    // Bosch BMP280 datasheet appendix 8.1, double-precision compensation.
    bool compensate(uint32_t rawP, uint32_t rawT, uint32_t& pressurePa, int16_t& temperatureCentiC) const {
        if (!p1 || rawP >= 0xfffff || rawT >= 0xfffff || rawP == 0x80000 || rawT == 0x80000) return false;
        double v1 = (double(rawT)/16384.0 - double(t1)/1024.0)*t2;
        double v2 = (double(rawT)/131072.0 - double(t1)/8192.0);
        double fine = v1 + v2*v2*t3;
        double temperature = fine/5120.0;
        v1 = fine/2.0 - 64000.0;
        v2 = v1*v1*p6/32768.0 + v1*p5*2.0;
        v2 = v2/4.0 + double(p4)*65536.0;
        v1 = (p3*v1*v1/524288.0 + p2*v1)/524288.0;
        v1 = (1.0 + v1/32768.0)*p1;
        if (v1 == 0) return false;
        double pressure = (1048576.0 - rawP - v2/4096.0)*6250.0/v1;
        pressure += (p9*pressure*pressure/2147483648.0 + pressure*p8/32768.0 + p7)/16.0;
        if (!isfinite(temperature) || !isfinite(pressure) || temperature < -40 || temperature > 85 || pressure < 30000 || pressure > 110000) return false;
        pressurePa = uint32_t(lround(pressure));
        temperatureCentiC = int16_t(lround(temperature*100));
        return true;
    }
};

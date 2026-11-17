#pragma once
#include <stdint.h>
#include <stddef.h>
constexpr int LOW = 0, HIGH = 1, INPUT = 0, OUTPUT = 1, SERIAL_8N1 = 0;
extern int aux, accesses;
extern uint32_t clockMs;
inline void digitalWrite(int, int) { ++accesses; }
inline void pinMode(int, int) { ++accesses; }
inline int digitalRead(int) { return aux; }
inline uint32_t millis() { return clockMs; }
struct FakeSerial {
    void begin(uint32_t, int, int, int) { ++accesses; }
    void end() { ++accesses; }
    int availableForWrite() { return 128; }
    size_t write(const uint8_t*, size_t size) { return size; }
};
extern FakeSerial Serial2;

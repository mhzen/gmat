#pragma once
#include <string.h>
#include "packet.h"

inline bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
    return uint32_t(now - since) >= interval;
}

inline int16_t roundedScale(int16_t raw, int32_t numerator, int32_t denominator) {
    int32_t value = int32_t(raw) * numerator;
    return int16_t((value + (value < 0 ? -denominator / 2 : denominator / 2)) / denominator);
}
inline int16_t accelerationMg(int16_t raw) { return roundedScale(raw, 1000, 2048); }
inline int16_t gyroTenths(int16_t raw) { return roundedScale(raw, 100, 164); }
inline int16_t signedWord(const uint8_t* bytes) {
    const uint16_t word = (uint16_t(bytes[0]) << 8) | bytes[1];
    return int16_t(word < 0x8000 ? int32_t(word) : int32_t(word) - 65536);
}

inline bool radioCommand(const uint8_t* bytes, size_t size, Command& command) {
    if (size != 1 || bytes[0] < 1 || bytes[0] > 3) return false;
    command = static_cast<Command>(bytes[0]);
    return true;
}

// LF terminates a line; CRLF is accepted. Overflow/binary input poisons the whole line.
struct SerialCommands {
    char line[8] = {};
    size_t length = 0;
    bool invalid = false;
    bool push(char c, Command& command, bool& complete) {
        complete = c == '\n';
        if (!complete) {
            if (length == sizeof(line) || c == '\0') invalid = true;
            else line[length++] = c;
            return false;
        }
        size_t size = length;
        if (size && line[size - 1] == '\r') --size;
        bool valid = !invalid;
        if (valid && size == 3 && memcmp(line, "ARM", 3) == 0) command = Command::ARM;
        else if (valid && size == 6 && memcmp(line, "DISARM", 6) == 0) command = Command::DISARM;
        else if (valid && size == 4 && memcmp(line, "FIRE", 4) == 0) command = Command::FIRE;
        else valid = false;
        length = 0;
        invalid = false;
        return valid;
    }
};

struct BenchState {
    State state = State::IDLE;
    bool fault = false;
    bool ejectReported = false;
    uint32_t ejectAt = 0;
    void fail() { fault = true; state = State::IDLE; ejectReported = false; }
    bool apply(Command command, uint32_t now) {
        if (command == Command::DISARM) { state = State::IDLE; ejectReported = false; return true; }
        if (fault) return false;
        if (command == Command::ARM && state == State::IDLE) { state = State::ARMED; return true; }
        if (command == Command::FIRE && state == State::ARMED) {
            state = State::EJECT; ejectAt = now; ejectReported = false; return true;
        }
        return false;
    }
    void tick(uint32_t now, uint32_t interval) {
        if (state == State::EJECT && ejectReported && elapsed(now, ejectAt, interval)) state = State::DESCENT;
    }
    void reported() { if (state == State::EJECT) ejectReported = true; }
    uint8_t flags() const { return state == State::ARMED ? 1 : 0; } // Actual-fired bit stays clear.
};

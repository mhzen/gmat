#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "packet.h"

// Binary frames avoid hex doubling: T + 28 + CRC = 31 bytes (fits one E32 packet), C + 1 + CRC = 4.
// No delimiter: a frame starts at its type byte and has a fixed length per type.
inline uint16_t frameCrc(const void* data, size_t size) {
    uint16_t crc = 0xffff;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        crc ^= uint16_t(bytes[i]) << 8;
        for (int b = 0; b < 8; ++b) crc = crc & 0x8000 ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
    }
    return crc;
}
inline size_t frameSize(char type, size_t size) {
    return ((type == 'T' && size == sizeof(Telemetry)) || (type == 'C' && size == 1)) ? 3 + size : 0;
}
inline size_t encodeFrame(char type, const void* data, size_t size, uint8_t* out) {
    const size_t count = frameSize(type, size);
    if (!count) return 0;
    out[0] = uint8_t(type);
    memcpy(out + 1, data, size);
    const uint16_t crc = frameCrc(out, 1 + size);
    out[1 + size] = uint8_t(crc >> 8);
    out[2 + size] = uint8_t(crc);
    return count;
}
// Parsing resyncs on the type byte alone; a truncated or corrupted frame costs one frame, not a loop.
struct RadioFrame {
    uint8_t line[1 + sizeof(Telemetry) + 2] = {};
    size_t length = 0;
    bool push(uint8_t byte, char expected, void* output, size_t size) {
        const size_t count = frameSize(expected, size);
        if (!count) return false;
        if (!length && byte != uint8_t(expected)) return false;
        line[length++] = byte;
        if (length < count) return false;
        const bool valid = uint16_t((line[count-2] << 8) | line[count-1]) == frameCrc(line, count - 2);
        length = 0;
        if (!valid) return false;
        memcpy(output, line + 1, size);
        return true;
    }
};

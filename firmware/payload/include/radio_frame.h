#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "packet.h"

// ASCII hex keeps newline framing independent of binary payload values.
// T + 56 hex digits + CRC16 + LF = 62 bytes; C + 2 + CRC16 + LF = 8.
inline uint16_t frameCrc(const char* text, size_t size) {
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < size; ++i) {
        crc ^= uint16_t(uint8_t(text[i])) << 8;
        for (int b = 0; b < 8; ++b) crc = crc & 0x8000 ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
    }
    return crc;
}
inline int unhex(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}
inline size_t encodeFrame(char type, const void* data, size_t size, char* out) {
    if (!((type == 'T' && size == sizeof(Telemetry)) || (type == 'C' && size == 1))) return 0;
    const char* hex = "0123456789ABCDEF";
    const auto* bytes = static_cast<const uint8_t*>(data);
    out[0] = type;
    for (size_t i = 0; i < size; ++i) {
        out[1 + i * 2] = hex[bytes[i] >> 4];
        out[2 + i * 2] = hex[bytes[i] & 15];
    }
    size_t length = 1 + size * 2;
    uint16_t crc = frameCrc(out, length);
    for (int shift = 12; shift >= 0; shift -= 4) out[length++] = hex[(crc >> shift) & 15];
    out[length++] = '\n';
    return length;
}
struct RadioFrame {
    char line[1 + sizeof(Telemetry)*2 + 4] = {};
    size_t length = 0;
    bool invalid = false;
    bool push(char c, char expected, void* output, size_t size) {
        if (c != '\n') {
            if (length == sizeof(line)) invalid = true;
            else line[length++] = c;
            return false;
        }
        bool valid = !invalid && ((expected == 'T' && size == sizeof(Telemetry)) || (expected == 'C' && size == 1)) &&
                     length == 1 + size * 2 + 4 && line[0] == expected;
        uint8_t bytes[sizeof(Telemetry)] = {};
        uint16_t crc = 0;
        if (valid) {
            for (size_t i = 1; i < length; ++i) if (unhex(line[i]) < 0) valid = false;
            if (valid) {
                for (size_t i = 0; i < size; ++i) bytes[i] = uint8_t(unhex(line[1+i*2])*16 + unhex(line[2+i*2]));
                for (size_t i = length-4; i < length; ++i) crc = uint16_t((crc << 4) | unhex(line[i]));
                valid = crc == frameCrc(line, length-4);
            }
        }
        if (valid) memcpy(output, bytes, size);
        length = 0;
        invalid = false;
        return valid;
    }
};

#include <assert.h>
#include <stdio.h>
#include <initializer_list>
#include "bench.h"
#include "radio_frame.h"
#include "barometer.h"

bool line(SerialCommands& parser, const char* text, Command& command) {
    bool valid = false, complete = false;
    for (const char* p = text; *p; ++p) valid = parser.push(*p, command, complete);
    return complete && valid;
}

int main() {
    BarometerCalibration calibration;
    calibration.t1=27504; calibration.t2=26435; calibration.t3=-1000;
    calibration.p1=36477; calibration.p2=-10685; calibration.p3=3024;
    calibration.p4=2855; calibration.p5=140; calibration.p6=-7;
    calibration.p7=15500; calibration.p8=-14600; calibration.p9=6000;
    uint32_t pressure; int16_t temperature;
    assert(calibration.compensate(415148, 519888, pressure, temperature));
    assert(pressure == 100653 && temperature == 2508);
    assert(!calibration.compensate(0x80000, 519888, pressure, temperature));
    assert(!calibration.compensate(415148, 0x80000, pressure, temperature));
    calibration.p1 = 0;
    assert(!calibration.compensate(415148, 519888, pressure, temperature));
    uint8_t blank[24] = {};
    assert(!calibration.load(blank));
    memset(blank, 255, sizeof(blank)); assert(!calibration.load(blank));
    assert(frameCrc("123456789", 9) == 0x29b1);
    char frame[62];
    uint8_t sent[28] = {}, received[28] = {};
    for (unsigned i = 0; i < 28; ++i) sent[i] = uint8_t(i * 11);
    assert(encodeFrame('T', sent, 28, frame) == 62);
    RadioFrame framing;
    for (unsigned i = 0; i < 62; ++i)
        assert(framing.push(frame[i], 'T', received, 28) == (i == 61));
    assert(memcmp(sent, received, 28) == 0);
    frame[4] = frame[4] == '0' ? '1' : '0';
    for (char c : frame) assert(!framing.push(c, 'T', received, 28));
    for (int i = 0; i < 100; ++i) assert(!framing.push('X', 'T', received, 28));
    assert(!framing.push('\n', 'T', received, 28));
    for (uint8_t cmd = 1; cmd <= 3; ++cmd) {
        size_t size = encodeFrame('C', &cmd, 1, frame);
        assert(size == 8);
        uint8_t actual = 0;
        for (size_t i = 0; i < size; ++i)
            assert(framing.push(frame[i], 'C', &actual, 1) == (i == size - 1));
        assert(actual == cmd);
    }
    assert(encodeFrame('X', sent, 28, frame) == 0);
    assert(encodeFrame('T', sent, 22, frame) == 0); // Reject old schema.
    assert(sizeof(Telemetry) == 28 && sizeof(CommandPacket) == 1);
    Telemetry packet = {0x04030201, 0, -1, 0, 0, 0, 0, 0, 4, 0, 100653, 2508};
    const auto* bytes = reinterpret_cast<const uint8_t*>(&packet);
    assert(bytes[0] == 1 && bytes[3] == 4 && bytes[8] == 255 && bytes[20] == 4);
    assert(accelerationMg(2048) == 1000 && accelerationMg(-2048) == -1000);
    assert(accelerationMg(32767) == 16000 && accelerationMg(-32768) == -16000);
    assert(accelerationMg(1) == 0 && accelerationMg(-1) == 0);
    assert(accelerationMg(2) == 1 && accelerationMg(-2) == -1);
    assert(gyroTenths(164) == 100 && gyroTenths(-164) == -100);
    assert(gyroTenths(1) == 1 && gyroTenths(-1) == -1);
    assert(gyroTenths(32767) == 19980 && gyroTenths(-32768) == -19980);
    const uint8_t minimum[] = {0x80, 0}, maximum[] = {0x7f, 0xff};
    assert(signedWord(minimum) == -32768 && signedWord(maximum) == 32767);
    Command command = Command::FIRE;
    for (uint8_t b = 0; b < 5; ++b) {
        assert(radioCommand(&b, 1, command) == (b >= 1 && b <= 3));
        assert(!radioCommand(&b, 0, command) && !radioCommand(&b, 2, command));
    }
    SerialCommands parser;
    assert(line(parser, "ARM\n", command) && command == Command::ARM);
    assert(line(parser, "DISARM\r\n", command) && command == Command::DISARM);
    assert(line(parser, "FIRE\n", command) && command == Command::FIRE);
    for (const char* bad : {"arm\n", " ARM\n", "FIRE \n", "ARM\rX\n", "\n", "XXXXXXXXARM\n"})
        assert(!line(parser, bad, command));
    bool complete;
    parser.push('\0', command, complete);
    assert(!line(parser, "ARM\n", command));
    assert(line(parser, "ARM\n", command)); // Recovery only after the bad line ends.
    BenchState bench;
    assert(bench.state == State::IDLE && bench.flags() == 0);
    assert(!bench.apply(Command::FIRE, 0));
    assert(bench.apply(Command::ARM, 0) && bench.flags() == 1);
    assert(!bench.apply(Command::ARM, 0));
    assert(bench.apply(Command::FIRE, 0xfffffff0) && bench.flags() == 0);
    bench.tick(184, 200); // Rollover, but EJECT must be reported at least once.
    assert(bench.state == State::EJECT);
    bench.reported();
    bench.tick(183, 200);
    assert(bench.state == State::EJECT);
    bench.tick(184, 200);
    assert(bench.state == State::DESCENT && !bench.apply(Command::ARM, 184));
    for (int state = 0; state <= 6; ++state) {
        bench.state = static_cast<State>(state);
        assert(bench.apply(Command::DISARM, 0) && bench.state == State::IDLE);
        bench.state = static_cast<State>(state);
        bench.fail();
        assert(bench.state == State::IDLE && bench.flags() == 0);
        assert(!bench.apply(Command::ARM, 0) && !bench.apply(Command::FIRE, 0));
        assert(bench.apply(Command::DISARM, 0) && bench.fault);
    }
    assert(!elapsed(183, 0xfffffff0, 200) && elapsed(184, 0xfffffff0, 200));
    puts("All payload bench checks passed");
}

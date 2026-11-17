#include <assert.h>
#include "bridge.h"
int main() {
    Telemetry p = {4294967295u, 0, -32768, 32767, 0, -1, 1, 0, 6, 3, 100653, 2508};
    assert(validTelemetry(p, 28) && !validTelemetry(p, 22));
    char line[128];
    csv(line, sizeof(line), p);
    assert(strcmp(line, "4294967295,0,-32768,32767,0,-1,1,0,6,3,100653,2508\n") == 0);
    p.state = 7; assert(!validTelemetry(p, 28));
    p.state = 0; p.flags = 4; assert(!validTelemetry(p, 28));
    p.flags = 0; p.pressure_pa = 29999; assert(!validTelemetry(p, 28));
    p.pressure_pa = 110001; assert(!validTelemetry(p, 28));
    p.pressure_pa = 100653; p.temperature_centi_c = -4001; assert(!validTelemetry(p, 28));
    p.temperature_centi_c = 8501; assert(!validTelemetry(p, 28));
    SerialCommands parser;
    Command c; bool complete;
    for (const char* s = "invalid\nARM\r\n"; *s; ++s) {
        bool valid = parser.push(*s, c, complete);
        if (valid) assert(complete && c == Command::ARM);
    }
    CommandPacket cmd = {uint8_t(Command::FIRE)};
    assert(cmd.cmd == 3);
    puts("All GCS bench checks passed");
}

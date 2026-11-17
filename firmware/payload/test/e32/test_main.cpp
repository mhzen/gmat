#include <assert.h>
#include "e32.h"
int aux = HIGH, accesses = 0;
uint32_t clockMs = 0;
FakeSerial Serial2;
int main() {
    E32 r;
    r.begin();
    assert(!r.enabled && accesses == 0); // Disabled means no hardware access.
    r.enabled = true;
    assert(r.poll(0) && !r.ready());
    assert(r.poll(100) && r.ready());
    uint8_t command = 1;
    clockMs = 100;
    assert(r.send('C', &command, 1) && r.busy && !r.ready());
    assert(r.poll(101) && r.busy);
    aux = LOW;
    assert(r.poll(110) && r.busy);
    aux = HIGH;
    assert(r.poll(120) && r.busy);
    assert(r.poll(122) && r.ready());
    aux = LOW;
    assert(r.poll(10000)); // Timeout starts at falling edge, not earlier rising edge.
    assert(!r.poll(12000));
    r.stop();
    assert(!r.ready());
    E32 wrap;
    wrap.enabled = true; wrap.started = 0xfffffff0;
    aux = HIGH;
    assert(wrap.poll(0xfffffff0));
    assert(wrap.poll(84) && wrap.ready());
}

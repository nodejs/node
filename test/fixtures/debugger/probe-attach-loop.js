'use strict';
// Increments a counter on a timer so probes have a changing value
// to read, and prints `PROBE_READY` once the timer is scheduled.
let count = 0;
function tick() {
  count++;
}

setInterval(tick, 50);
console.log('PROBE_READY');

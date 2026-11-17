'use strict';
// Runs tick() continuously, so a breakpoint in tick() pauses the target as soon as it is bound.
let count = 0;
function tick() {
  count++;
  setImmediate(tick);
}

tick();
console.log('PROBE_READY');

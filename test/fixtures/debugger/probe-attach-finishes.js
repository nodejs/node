'use strict';
// Runs tick() once when its stdin is closed, or throw if PROBE_ATTACH_THROW is set.
let count = 0;
function tick() {
  count++;
  if (process.env.PROBE_ATTACH_THROW) throw new Error('boom');
}

process.stdin.on('end', tick);
process.stdin.resume();
console.log('PROBE_READY');

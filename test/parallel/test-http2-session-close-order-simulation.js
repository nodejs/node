'use strict';
// Simulates the nextTick ordering in closeSession to validate the fix logic
// without requiring a compiled binary.

const assert = require('assert');

function simulate(label, handleFirst) {
  const events = [];
  const ticks = [];

  function nextTick(fn) { ticks.push(fn); }
  function drainTicks() { while (ticks.length) ticks.shift()(); }

  // stream.destroy() schedules stream 'close' on nextTick
  function destroyStream() {
    nextTick(() => events.push('stream close'));
  }

  // finishSessionClose (socket already destroyed) schedules session 'close' on nextTick
  function finishSessionClose() {
    nextTick(() => events.push('session close'));
  }

  // handle.destroy() calls ondone synchronously when socket is already destroyed
  // and there are no writes in progress (the scenario from the bug report)
  function destroyHandle(ondone) {
    ondone(); // synchronous, as the C++ Http2Session::Close does
  }

  function closeSession() {
    if (handleFirst) {
      // Our fix: destroy handle first → session 'close' queued first
      destroyHandle(finishSessionClose);
      destroyStream();
    } else {
      // Old code: destroy streams first → stream 'close' queued first
      destroyStream();
      destroyHandle(finishSessionClose);
    }
  }

  closeSession();
  drainTicks();
  return events;
}

const before = simulate('BEFORE fix', false);
const after  = simulate('AFTER fix',  true);

console.log('BEFORE fix ordering:', before.join(' -> '));
console.log('AFTER fix  ordering:', after.join(' -> '));

// Old ordering: stream close fires before session close (the bug)
assert.deepStrictEqual(before, ['stream close', 'session close'],
  'Expected old code to have wrong order (stream before session)');

// New ordering: session close fires first (the fix)
assert.deepStrictEqual(after, ['session close', 'stream close'],
  'session "close" must fire before stream "close"');

console.log('OK');

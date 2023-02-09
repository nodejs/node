'use strict';

const common = require('../common');
const assert = require('assert');
const { performance } = require('perf_hooks');

const { nodeTiming } = performance;
assert.strictEqual(nodeTiming.name, 'node');
assert.strictEqual(nodeTiming.entryType, 'node');

assert.strictEqual(nodeTiming.startTime, 0);
const now = performance.now();
assert.ok(nodeTiming.duration >= now);

// Check that the nodeTiming milestone values are in the correct order and greater than 0.
const keys = ['nodeStart', 'v8Start', 'environment', 'bootstrapComplete'];
for (let idx = 0; idx < keys.length; idx++) {
  if (idx === 0) {
    assert.ok(nodeTiming[keys[idx]] >= 0);
    continue;
  }
  assert.ok(nodeTiming[keys[idx]] > nodeTiming[keys[idx - 1]], `expect nodeTiming['${keys[idx]}'] > nodeTiming['${keys[idx - 1]}']`);
}

// loop milestones.
assert.strictEqual(nodeTiming.idleTime, 0);
assert.strictEqual(nodeTiming.loopStart, -1);
assert.strictEqual(nodeTiming.loopExit, -1);

setTimeout(common.mustCall(() => {
  assert.ok(nodeTiming.idleTime >= 0);
  assert.ok(nodeTiming.idleTime + nodeTiming.loopExit <= nodeTiming.duration);
  assert.ok(nodeTiming.loopStart >= nodeTiming.bootstrapComplete);
}, 1), 1);

// Can not be wrapped in common.mustCall().
process.on('exit', () => {
  assert.ok(nodeTiming.loopExit > 0);
});

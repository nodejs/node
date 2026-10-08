'use strict';

const common = require('../common');
const assert = require('assert');
const { performance } = require('perf_hooks');
const { isMainThread } = require('worker_threads');

const { nodeTiming } = performance;
assert.strictEqual(nodeTiming.name, 'node');
assert.strictEqual(nodeTiming.entryType, 'node');

assert.strictEqual(nodeTiming.startTime, 0);

// uvMetricsInfoBigInt holds bigint values, which JSON.stringify() cannot
// serialize, so it must not be part of the JSON representation, nor be copied
// along with the enumerable properties.
assert.strictEqual(typeof nodeTiming.uvMetricsInfoBigInt.loopCount, 'bigint');
assert.strictEqual(
  Object.getOwnPropertyDescriptor(nodeTiming, 'uvMetricsInfoBigInt').enumerable,
  false);
assert.strictEqual(
  Object.hasOwn(JSON.parse(JSON.stringify(nodeTiming)), 'uvMetricsInfoBigInt'),
  false);
assert.strictEqual(
  Object.hasOwn(JSON.parse(JSON.stringify(performance)).nodeTiming,
                'uvMetricsInfoBigInt'),
  false);
{
  const copy = { ...nodeTiming };
  assert.strictEqual(Object.hasOwn(copy, 'uvMetricsInfoBigInt'), false);
  assert.strictEqual(typeof JSON.stringify(copy), 'string');
}
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
if (isMainThread) {
  // Main thread does not start loop until the first tick is finished.
  assert.strictEqual(nodeTiming.loopStart, -1);
} else {
  // Worker threads run the user script after loop is started.
  assert.ok(nodeTiming.loopStart >= nodeTiming.bootstrapComplete);
}
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

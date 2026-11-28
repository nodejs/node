// Flags: --expose-gc --sampling-heap-profiler-suppress-randomness
'use strict';

require('../common');
const assert = require('assert');
const v8 = require('v8');

function allocate() {
  return Array.from({ length: 512 }, () => new Array(64).fill(42));
}

const handle = v8.startHeapProfile({
  sampleInterval: 1024,
  includeObjectsCollectedByMajorGC: true,
  includeObjectsCollectedByMinorGC: true,
});

for (const value of [undefined, null, true, '1024', {}, 1n]) {
  assert.throws(() => handle.setSampleInterval(value), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}
for (const value of [0, -1, 1.5, NaN, Infinity, Number.MAX_SAFE_INTEGER + 1]) {
  assert.throws(() => handle.setSampleInterval(value), {
    code: 'ERR_OUT_OF_RANGE',
  });
}

const retained = allocate();
const before = handle.getSamples();
assert(before.length > 0);
for (const sample of before) {
  assert.strictEqual(sample.sampleInterval, 1024);
  assert.strictEqual(sample.size, sample.objectSize * sample.objectCount);
  assert.strictEqual(typeof sample.nodeId, 'number');
  assert.strictEqual(typeof sample.ordinal, 'number');
  assert.strictEqual(typeof sample.isLive, 'boolean');
}

assert.strictEqual(handle.setSampleInterval(4096), undefined);
const moreRetained = allocate();
const after = handle.getSamples();
assert(after.some((sample) => sample.sampleInterval === 4096));
const byOrdinal = new Map(after.map((sample) => [sample.ordinal, sample]));
for (const sample of before) {
  const current = byOrdinal.get(sample.ordinal);
  assert(current, `Sample ${sample.ordinal} was lost after changing the interval`);
  assert.strictEqual(current.sampleInterval, sample.sampleInterval);
  assert.strictEqual(current.objectSize, sample.objectSize);
  assert.strictEqual(current.objectCount, sample.objectCount);
}

global.gc();
const afterGC = handle.getSamples();
assert(afterGC.some((sample) => !sample.isLive));
assert(afterGC.some((sample) => sample.sampleInterval === 1024));
assert(afterGC.some((sample) => sample.sampleInterval === 4096));

const profile = JSON.parse(handle.stop());
const saved = new Map(profile.samples.map((sample) => [sample.ordinal, sample]));
const nodeIds = new Set();
function visit(node) {
  nodeIds.add(node.id);
  node.children.forEach(visit);
}
visit(profile.head);
for (const sample of afterGC) {
  const current = saved.get(sample.ordinal);
  assert(current);
  assert.strictEqual(current.sampleInterval, sample.sampleInterval);
  assert.strictEqual(current.size, sample.size);
  assert(nodeIds.has(sample.nodeId));
}
assert.strictEqual(retained.length, 512);
assert.strictEqual(moreRetained.length, 512);

// A stopped handle must not poll or update a new profiling session.
const next = v8.startHeapProfile({ sampleInterval: 1024 });
assert.throws(() => handle.getSamples(), {
  code: 'ERR_HEAP_PROFILE_NOT_STARTED',
});
assert.throws(() => handle.setSampleInterval(1), {
  code: 'ERR_HEAP_PROFILE_NOT_STARTED',
});
const nextRetained = allocate();
const nextSamples = next.getSamples();
assert(nextSamples.length > 0);
assert(nextSamples.every((sample) => sample.sampleInterval === 1024));
assert.strictEqual(nextRetained.length, 512);
next[Symbol.dispose]();
assert.throws(() => next.getSamples(), {
  code: 'ERR_HEAP_PROFILE_NOT_STARTED',
});

// Polling honors the forceGC option and retains collected samples when enabled.
const forceGC = v8.startHeapProfile({
  sampleInterval: 1024,
  forceGC: true,
  includeObjectsCollectedByMajorGC: true,
  includeObjectsCollectedByMinorGC: true,
});
allocate();
assert(forceGC.getSamples().some((sample) => !sample.isLive));
forceGC.stop();

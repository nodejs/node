'use strict';

require('../common');
const assert = require('assert');
const v8 = require('v8');

assert.throws(() => v8.startHeapProfile('bad'), {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => v8.startHeapProfile({ sampleInterval: '1024' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => v8.startHeapProfile({ sampleInterval: 1.1 }), {
  code: 'ERR_OUT_OF_RANGE',
});
assert.throws(() => v8.startHeapProfile({ sampleInterval: 0 }), {
  code: 'ERR_OUT_OF_RANGE',
});
assert.throws(() => v8.startHeapProfile({ sampleInterval: -1 }), {
  code: 'ERR_OUT_OF_RANGE',
});


assert.throws(() => v8.startHeapProfile({ stackDepth: '16' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => v8.startHeapProfile({ stackDepth: 1.1 }), {
  code: 'ERR_OUT_OF_RANGE',
});
assert.throws(() => v8.startHeapProfile({ stackDepth: -1 }), {
  code: 'ERR_OUT_OF_RANGE',
});

assert.throws(() => v8.startHeapProfile({ forceGC: 'true' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(
  () => v8.startHeapProfile({ includeObjectsCollectedByMajorGC: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
assert.throws(
  () => v8.startHeapProfile({ includeObjectsCollectedByMinorGC: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

const invalidLimits = [-1, 0, '', {}, NaN, undefined];
for (const value of invalidLimits) {
  assert.throws(() => v8.setHeapProfileNearHeapLimit(value),
                /ERR_INVALID_ARG_TYPE|ERR_OUT_OF_RANGE/);
}

// Default params.
{
  const handle = v8.startHeapProfile();
  const profile = handle.stop();
  JSON.parse(profile);
}

// Custom params with all flags.
{
  const handle = v8.startHeapProfile({
    sampleInterval: 1024,
    stackDepth: 8,
    forceGC: true,
    includeObjectsCollectedByMajorGC: true,
    includeObjectsCollectedByMinorGC: true,
  });
  assert.throws(() => v8.startHeapProfile(), {
    code: 'ERR_HEAP_PROFILE_HAVE_BEEN_STARTED',
  });
  const profile = handle.stop();
  JSON.parse(profile);
}

{
  const handle = v8.startHeapProfile({ sampleInterval: 1, stackDepth: 8 });
  const retained = [];
  for (let i = 0; i < 4096; i++) {
    retained.push({ i, payload: new Array(16).fill(i) });
  }
  const parsed = JSON.parse(handle.stop());

  assert.strictEqual(typeof parsed.head.selfSize, 'number');
  assert.strictEqual(typeof parsed.head.selfCount, 'number');
  assert.strictEqual(typeof parsed.head.id, 'number');
  assert.strictEqual(typeof parsed.head.callFrame, 'object');

  assert(parsed.samples.length > 0);
  for (const sample of parsed.samples) {
    assert.strictEqual(typeof sample.size, 'number');
    assert.strictEqual(typeof sample.objectSize, 'number');
    assert.strictEqual(typeof sample.objectCount, 'number');
    assert.strictEqual(typeof sample.nodeId, 'number');
    assert.strictEqual(typeof sample.ordinal, 'number');
    assert.strictEqual(typeof sample.isLive, 'boolean');
    assert.strictEqual(sample.size, sample.objectSize * sample.objectCount);
  }
}

// Second stop returns undefined.
{
  const handle = v8.startHeapProfile();
  JSON.parse(handle.stop());
  assert.strictEqual(handle.stop(), undefined);
}

// Profile and snapshot near-heap-limit callbacks coexist.
{
  v8.setHeapProfileNearHeapLimit(1);
  v8.setHeapProfileNearHeapLimit(1);   // no-op
  v8.setHeapSnapshotNearHeapLimit(1);
  v8.setHeapSnapshotNearHeapLimit(1);  // no-op
}

'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
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

assert.throws(() => v8.setHeapProfileNearHeapLimit(), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => v8.setHeapProfileNearHeapLimit(() => {}, {
  maxExtensions: 0,
}), {
  code: 'ERR_OUT_OF_RANGE',
});
assert.throws(() => v8.setHeapProfileNearHeapLimit(() => {}, {
  extensionSize: 0,
}), {
  code: 'ERR_OUT_OF_RANGE',
});

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

// Second stop returns undefined.
{
  const handle = v8.startHeapProfile();
  JSON.parse(handle.stop());
  assert.strictEqual(handle.stop(), undefined);
}

// Profile and snapshot near-heap-limit callbacks coexist.
{
  v8.setHeapProfileNearHeapLimit(() => {});
  v8.setHeapProfileNearHeapLimit(() => {});  // no-op
  v8.setHeapSnapshotNearHeapLimit(1);
  v8.setHeapSnapshotNearHeapLimit(1);        // no-op
}

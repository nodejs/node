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

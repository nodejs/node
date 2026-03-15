'use strict';

require('../common');
const assert = require('assert');
const v8 = require('v8');
const { heapProfilerConstants } = v8;

assert.ok(Object.isFrozen(heapProfilerConstants));
assert.strictEqual(typeof heapProfilerConstants.SAMPLING_NO_FLAGS, 'number');
assert.strictEqual(typeof heapProfilerConstants.SAMPLING_FORCE_GC, 'number');
assert.strictEqual(
  typeof heapProfilerConstants.SAMPLING_INCLUDE_OBJECTS_COLLECTED_BY_MAJOR_GC,
  'number');
assert.strictEqual(
  typeof heapProfilerConstants.SAMPLING_INCLUDE_OBJECTS_COLLECTED_BY_MINOR_GC,
  'number');

function assertInvalidStartHeapProfile(args, code) {
  assert.throws(() => Reflect.apply(v8.startHeapProfile, undefined, args), {
    code,
  });
  // Verify the invalid call above did not accidentally start profiling.
  const handle = v8.startHeapProfile();
  const profile = handle.stop();
  assert.ok(typeof profile === 'string');
  assert.ok(profile.length > 0);
}

assertInvalidStartHeapProfile(['1024'], 'ERR_INVALID_ARG_TYPE');
assertInvalidStartHeapProfile([1.1], 'ERR_OUT_OF_RANGE');
assertInvalidStartHeapProfile([0], 'ERR_OUT_OF_RANGE');
assertInvalidStartHeapProfile([-1], 'ERR_OUT_OF_RANGE');

assertInvalidStartHeapProfile([1024, '16'], 'ERR_INVALID_ARG_TYPE');
assertInvalidStartHeapProfile([1024, 1.1], 'ERR_OUT_OF_RANGE');
assertInvalidStartHeapProfile([1024, -1], 'ERR_OUT_OF_RANGE');

assertInvalidStartHeapProfile([1024, 16, '0'], 'ERR_INVALID_ARG_TYPE');
assertInvalidStartHeapProfile([1024, 16, -1], 'ERR_OUT_OF_RANGE');
assertInvalidStartHeapProfile([1024, 16, 8], 'ERR_OUT_OF_RANGE');

// Default params.
{
  const handle = v8.startHeapProfile();
  const profile = handle.stop();
  JSON.parse(profile);
}

// Custom params.
{
  const handle = v8.startHeapProfile(
    1024,
    8,
    heapProfilerConstants.SAMPLING_FORCE_GC |
      heapProfilerConstants.SAMPLING_INCLUDE_OBJECTS_COLLECTED_BY_MAJOR_GC |
      heapProfilerConstants.SAMPLING_INCLUDE_OBJECTS_COLLECTED_BY_MINOR_GC,
  );
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

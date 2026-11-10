'use strict';

const common = require('../common');
const assert = require('node:assert');

for (const FloatArray of [Float16Array, Float32Array, Float64Array]) {
  const actual = new FloatArray([0, 1.5, 2.5, 0]).subarray(1, 3);
  const equal = new FloatArray([1.5, 2.5]);
  const different = new FloatArray([1.5, 3.5]);
  const shorter = new FloatArray([1.5]);
  const empty = new FloatArray(0);
  const emptyEqual = new FloatArray(0);

  // Check strict comparisons before installing the loose-mode getter guards.
  assert.deepStrictEqual(actual, equal);
  assert.notDeepStrictEqual(actual, different);
  assert.notDeepStrictEqual(actual, shorter);
  assert.notDeepStrictEqual(actual, empty);
  assert.deepStrictEqual(empty, emptyEqual);

  // Own properties must not replace the view's intrinsic element count.
  for (const view of [actual, equal, different, shorter, empty, emptyEqual]) {
    Object.defineProperty(view, 'length', {
      get: common.mustNotCall('length getter'),
    });
    Object.defineProperty(view, 'byteLength', {
      get: common.mustNotCall('byteLength getter'),
    });
  }

  // eslint-disable-next-line no-restricted-properties
  assert.deepEqual(actual, equal);
  // eslint-disable-next-line no-restricted-properties
  assert.notDeepEqual(actual, different);
  // eslint-disable-next-line no-restricted-properties
  assert.notDeepEqual(actual, shorter);
  // eslint-disable-next-line no-restricted-properties
  assert.notDeepEqual(actual, empty);
  // eslint-disable-next-line no-restricted-properties
  assert.deepEqual(empty, emptyEqual);
}

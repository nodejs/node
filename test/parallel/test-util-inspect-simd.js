// Flags: --harmony_simd
'use strict';

require('../common');
const assert = require('assert');
const inspect = require('util').inspect;

const SIMD = global.SIMD;  // Pacify eslint.

assert.strictEqual(
    inspect(SIMD.Bool16x8()),
    'Bool16x8 [ false, false, false, false, false, false, false, false ]');

assert.strictEqual(
    inspect(SIMD.Bool32x4()),
    'Bool32x4 [ false, false, false, false ]');

assert.strictEqual(
    inspect(SIMD.Bool8x16()),
    'Bool8x16 [\r\n  false,\r\n  false,\r\n  false,\r\n  false,\r\n' +
    '  false,\r\n  false,\r\n  false,\r\n  false,\r\n  false,\r\n' +
    '  false,\r\n  false,\r\n  false,\r\n  false,\r\n  false,\r\n' +
    '  false,\r\n  false ]');

assert.strictEqual(
    inspect(SIMD.Bool32x4()),
    'Bool32x4 [ false, false, false, false ]');

assert.strictEqual(
    inspect(SIMD.Float32x4()),
    'Float32x4 [ NaN, NaN, NaN, NaN ]');

assert.strictEqual(
    inspect(SIMD.Int16x8()),
    'Int16x8 [ 0, 0, 0, 0, 0, 0, 0, 0 ]');

assert.strictEqual(
    inspect(SIMD.Int32x4()),
    'Int32x4 [ 0, 0, 0, 0 ]');

assert.strictEqual(
    inspect(SIMD.Int8x16()),
    'Int8x16 [ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]');

// The SIMD types below are not available in v5.
if (typeof SIMD.Uint16x8 === 'function') {
  assert.strictEqual(
      inspect(SIMD.Uint16x8()),
      'Uint16x8 [ 0, 0, 0, 0, 0, 0, 0, 0 ]');
}

if (typeof SIMD.Uint32x4 === 'function') {
  assert.strictEqual(
      inspect(SIMD.Uint32x4()),
      'Uint32x4 [ 0, 0, 0, 0 ]');
}

if (typeof SIMD.Uint8x16 === 'function') {
  assert.strictEqual(
      inspect(SIMD.Uint8x16()),
      'Uint8x16 [ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ]');
}

// Tests from test-inspect.js that should not fail with --harmony_simd.
assert.strictEqual(inspect([]), '[]');
assert.strictEqual(inspect([0]), '[ 0 ]');
assert.strictEqual(inspect({}), '{}');
assert.strictEqual(inspect({foo: 42}), '{ foo: 42 }');
assert.strictEqual(inspect(null), 'null');
assert.strictEqual(inspect(true), 'true');
assert.strictEqual(inspect(false), 'false');

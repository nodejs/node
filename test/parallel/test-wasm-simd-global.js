'use strict';

// Test that v128 SIMD globals work correctly end-to-end on all platforms,
// including big-endian (s390x). Specifically:
// 1. v128.const initializer -> global.get -> i32x4.extract_lane round-trip
// 2. i32x4.replace_lane -> global.set -> global.get -> i32x4.extract_lane round-trip
//
// This is a regression check for potential big-endian issues in V8's handling
// of v128 globals (e.g. s390x), independent of the ESM import path.

const common = require('../common');
const assert = require('assert');

// WAT source:
// (module
//   (global $g (mut v128) (v128.const i32x4 1 2 3 4))
//   (func (export "get0") (result i32) global.get $g i32x4.extract_lane 0)
//   (func (export "get1") (result i32) global.get $g i32x4.extract_lane 1)
//   (func (export "get2") (result i32) global.get $g i32x4.extract_lane 2)
//   (func (export "get3") (result i32) global.get $g i32x4.extract_lane 3)
//   (func (export "set") (param i32 i32 i32 i32)
//     i32.const 0 i32x4.splat
//     local.get 0 i32x4.replace_lane 0
//     local.get 1 i32x4.replace_lane 1
//     local.get 2 i32x4.replace_lane 2
//     local.get 3 i32x4.replace_lane 3
//     global.set $g)
// )
const wasmBytes = new Uint8Array([
  0, 97, 115, 109, 1, 0, 0, 0, 1, 12, 2, 96, 0, 1, 127, 96, 4, 127, 127,
  127, 127, 0, 3, 6, 5, 0, 0, 0, 0, 1, 6, 22, 1, 123, 1, 253, 12, 1, 0, 0,
  0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0, 11, 7, 35, 5, 4, 103, 101, 116, 48,
  0, 0, 4, 103, 101, 116, 49, 0, 1, 4, 103, 101, 116, 50, 0, 2, 4, 103, 101,
  116, 51, 0, 3, 3, 115, 101, 116, 0, 4, 10, 62, 5, 7, 0, 35, 0, 253, 27, 0,
  11, 7, 0, 35, 0, 253, 27, 1, 11, 7, 0, 35, 0, 253, 27, 2, 11, 7, 0, 35, 0,
  253, 27, 3, 11, 28, 0, 65, 0, 253, 17, 32, 0, 253, 28, 0, 32, 1, 253, 28,
  1, 32, 2, 253, 28, 2, 32, 3, 253, 28, 3, 36, 0, 11, 0, 11, 4, 110, 97,
  109, 101, 7, 4, 1, 0, 1, 103,
]);

if (!WebAssembly.validate(wasmBytes)) {
  common.skip('WebAssembly SIMD not supported');
}

const { exports: { get0, get1, get2, get3, set } } =
  new WebAssembly.Instance(new WebAssembly.Module(wasmBytes));

// Test 1: v128.const initializer lanes are read back correctly.
assert.strictEqual(get0(), 1, 'v128.const lane 0 should be 1');
assert.strictEqual(get1(), 2, 'v128.const lane 1 should be 2');
assert.strictEqual(get2(), 3, 'v128.const lane 2 should be 3');
assert.strictEqual(get3(), 4, 'v128.const lane 3 should be 4');

// Test 2: replace_lane -> global.set -> global.get -> extract_lane round-trip.
set(10, 20, 30, 40);
assert.strictEqual(get0(), 10, 'after set, lane 0 should be 10');
assert.strictEqual(get1(), 20, 'after set, lane 1 should be 20');
assert.strictEqual(get2(), 30, 'after set, lane 2 should be 30');
assert.strictEqual(get3(), 40, 'after set, lane 3 should be 40');

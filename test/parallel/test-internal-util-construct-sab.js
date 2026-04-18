// Flags: --enable-sharedarraybuffer-per-context --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { kMaxLength } = require('buffer');
const { isSharedArrayBuffer } = require('util/types');
const { constructSharedArrayBuffer } = require('internal/util');

// We're testing that we can construct a SAB even when the global is not exposed.
assert.strictEqual(typeof SharedArrayBuffer, 'undefined');

for (const length of [undefined, 0, 1, 2 ** 16]) {
  assert(isSharedArrayBuffer(constructSharedArrayBuffer(length)));
}

// Specifically test the following cases:
// - out-of-range allocation requests should not crash the process
// - no int64 overflow
for (const length of [-1, kMaxLength + 1, 2 ** 64]) {
  assert.throws(() => constructSharedArrayBuffer(length), RangeError);
}

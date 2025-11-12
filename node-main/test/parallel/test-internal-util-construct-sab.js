// Flags: --enable-sharedarraybuffer-per-context --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { isSharedArrayBuffer } = require('util/types');
const { constructSharedArrayBuffer } = require('internal/util');

// We're testing that we can construct a SAB even when the global is not exposed.
assert.strictEqual(typeof SharedArrayBuffer, 'undefined');

for (const length of [undefined, 0, 1, 2 ** 32]) {
  assert(isSharedArrayBuffer(constructSharedArrayBuffer(length)));
}

for (const length of [-1, Number.MAX_SAFE_INTEGER + 1, 2 ** 64]) {
  assert.throws(() => constructSharedArrayBuffer(length), RangeError);
}

'use strict';
const common = require('../common');
const assert = require('assert');
const ffi = require('node:ffi');

function assertIsPointer(thing) {
  assert.strictEqual(typeof thing, 'bigint');
  assert.ok(thing > 0n);
}

const buffer = Buffer.alloc(8);
const arrayBufferViews = common.getArrayBufferViews(buffer);

assertIsPointer(ffi.getBufferPointer(buffer));
for (const view of arrayBufferViews) {
  assertIsPointer(ffi.getBufferPointer(view));
}
assertIsPointer(ffi.getBufferPointer(new DataView(new ArrayBuffer(8))));
assertIsPointer(ffi.getBufferPointer(new ArrayBuffer(8)));
assertIsPointer(ffi.getBufferPointer(new SharedArrayBuffer(8)));

[
  7,
  '7',
  {},
  () => {},
  null,
  undefined,
  Symbol('7'),
  true,
].forEach((value) => {
  assert.throws(() => {
    ffi.getBufferPointer(value);
  }, { code: 'ERR_INVALID_ARG_TYPE' });
});

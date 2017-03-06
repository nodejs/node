'use strict';

// Flags: --expose-internals

// Confirm that if a custom ArrayBufferAllocator does not define a zeroFill
// property, that the buffer module will zero-fill when allocUnsafe() is called.

require('../common');

const assert = require('assert');
const buffer = require('buffer');

// Monkey-patch setupBufferJS() to have an undefined zeroFill.
const process = require('process');
const originalBinding = process.binding;

const binding = originalBinding('buffer');
const originalSetup = binding.setupBufferJS;

binding.setupBufferJS = (proto, obj) => {
  originalSetup(proto, obj);
  assert.strictEqual(obj.zeroFill[0], 1);
  delete obj.zeroFill;
};

const bindingObj = {};

binding.setupBufferJS(Buffer.prototype, bindingObj);
assert.strictEqual(bindingObj.zeroFill, undefined);

process.binding = (bindee) => {
  if (bindee === 'buffer')
    return binding;
  return originalBinding(bindee);
};

// Load from file system because internal buffer is already loaded and we're
// testing code that runs on first load only.
// Do not move this require() to top of file. It is important that
// `process.binding('buffer').setupBufferJS` be monkey-patched before this runs.
const monkeyPatchedBuffer = require('../../lib/buffer');

// On unpatched buffer, allocUnsafe() should not zero fill memory. It's always
// possible that a segment of memory is already zeroed out, so try again and
// again until we succeed or we time out.
let uninitialized = buffer.Buffer.allocUnsafe(1024);
while (uninitialized.some((val) => val !== 0))
  uninitialized = buffer.Buffer.allocUnsafe(1024);

// On monkeypatched buffer, zeroFill property is undefined. allocUnsafe() should
// zero-fill in that case.
const zeroFilled = monkeyPatchedBuffer.Buffer.allocUnsafe(1024);
assert(zeroFilled.every((val) => val === 0));

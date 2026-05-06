// Flags: --experimental-ffi --expose-internals
'use strict';
const common = require('../common');
common.skipIfFFIMissing();

const assert = require('node:assert');
const { test } = require('node:test');

// Capture the unpatched method BEFORE requiring node:ffi.
// ffi-fastcall.js patches DynamicLibrary.prototype.getFunction when
// node:ffi is loaded; grabbing the raw binding first lets us call
// getFunction without the wrapper, so we can inspect the Symbol-keyed
// metadata that the fast-call path attaches.
const { internalBinding } = require('internal/test/binding');
const ffiBinding = internalBinding('ffi');
const rawGetFunction = ffiBinding.DynamicLibrary.prototype.getFunction;

require('node:ffi');  // load AFTER capturing the unpatched method
const { libraryPath } = require('./ffi-test-common');

const fastcallEnabled = ffiBinding.kFastcallAlive !== undefined;

test('eligible signature has fastcall metadata', { skip: !fastcallEnabled }, () => {
  const lib = new ffiBinding.DynamicLibrary(libraryPath);
  try {
    const raw = rawGetFunction.call(lib, 'add_i32',
      { result: 'i32', parameters: ['i32', 'i32'] });
    assert.notStrictEqual(raw[ffiBinding.kFastcallAlive], undefined);
    assert.notStrictEqual(raw[ffiBinding.kFastcallInvokeSlow], undefined);
    assert.notStrictEqual(raw[ffiBinding.kFastcallParams], undefined);
    assert.notStrictEqual(raw[ffiBinding.kFastcallResult], undefined);
  } finally {
    lib.close();
  }
});

test('signature with function type lacks fastcall metadata',
  { skip: !fastcallEnabled }, () => {
  const lib = new ffiBinding.DynamicLibrary(libraryPath);
  try {
    // Use a real fixture symbol but declare the signature with a function-typed
    // arg. Eligibility should reject this; the inner v8::Function is plain
    // (no fastcall metadata).
    const raw = rawGetFunction.call(lib, 'add_i32',
      { result: 'i32', parameters: ['function', 'i32'] });
    assert.strictEqual(raw[ffiBinding.kFastcallAlive], undefined);
  } finally {
    lib.close();
  }
});

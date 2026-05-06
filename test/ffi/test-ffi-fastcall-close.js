// Flags: --experimental-ffi
'use strict';
const common = require('../common');
common.skipIfFFIMissing();

const assert = require('node:assert');
const { test } = require('node:test');
const ffi = require('node:ffi');
const { libraryPath } = require('./ffi-test-common');

test('close throws ERR_FFI_LIBRARY_CLOSED on subsequent fast-call', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
  });
  // Warm up TurboFan with many calls.
  for (let i = 0; i < 10000; i++) {
    functions.add_i32(i, i);
  }
  lib.close();
  assert.throws(() => functions.add_i32(1, 2),
                { code: 'ERR_FFI_LIBRARY_CLOSED' });
});

test('close throws ERR_FFI_LIBRARY_CLOSED via libffi slow path (ineligible signature)', () => {
  // 'function' as a parameter type makes IsFastCallEligible return false, so
  // the fast-call wrapper is never applied and the function dispatches directly
  // to InvokeFunction (libffi slow path). This exercises InvokeFunction's own
  // fn->closed check rather than the JS wrapper's alive-buffer check.
  // We declare return_pointer_arg with a 'function' param even though its real
  // ABI takes a void*; the closed check fires before any ABI work, so the
  // mismatch doesn't matter.
  const { lib, functions } = ffi.dlopen(libraryPath, {
    return_pointer_arg: { result: 'pointer', parameters: ['function'] },
  });
  // Don't call pre-close: the ABI-mismatched signature would produce garbage.
  lib.close();
  assert.throws(() => functions.return_pointer_arg(0n),
                { code: 'ERR_FFI_LIBRARY_CLOSED' });
});

test('ERR_FFI_LIBRARY_CLOSED is an Error instance with the right name', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
  });
  lib.close();
  let caught;
  try {
    functions.add_i32(1, 2);
  } catch (err) {
    caught = err;
  }
  assert.ok(caught instanceof Error);
  assert.strictEqual(caught.code, 'ERR_FFI_LIBRARY_CLOSED');
  assert.strictEqual(caught.name, 'Error');
  assert.strictEqual(caught.message, 'Library is closed');
});

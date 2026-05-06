// Flags: --experimental-ffi
'use strict';
const common = require('../common');
common.skipIfFFIMissing();

const assert = require('node:assert');
const { test } = require('node:test');
const ffi = require('node:ffi');
const { libraryPath } = require('./ffi-test-common');

test('0 args (getpid-like)', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    char_is_signed: { result: 'i32', parameters: [] },
  });
  try {
    const v = functions.char_is_signed();
    assert.strictEqual(typeof v, 'number');
    // char_is_signed returns 0 or 1 depending on platform.
    assert.ok(v === 0 || v === 1);
  } finally { lib.close(); }
});

test('0-arg function rejects extra args', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    char_is_signed: { result: 'i32', parameters: [] },
  });
  try {
    assert.throws(() => functions.char_is_signed(1),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => functions.char_is_signed(1, 2),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally { lib.close(); }
});

test('3 GP args', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    sum_3_i32: { result: 'i32', parameters: ['i32', 'i32', 'i32'] },
  });
  try {
    assert.strictEqual(functions.sum_3_i32(1, 2, 3), 6);
  } finally { lib.close(); }
});

test('4 GP args', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    sum_4_i32: { result: 'i32', parameters: ['i32', 'i32', 'i32', 'i32'] },
  });
  try {
    assert.strictEqual(functions.sum_4_i32(1, 2, 3, 4), 10);
  } finally { lib.close(); }
});

test('wrong argument count throws', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_i32: { result: 'i32', parameters: ['i32', 'i32'] },
  });
  try {
    assert.throws(() => functions.add_i32(1),
                  { code: 'ERR_INVALID_ARG_VALUE' });
    assert.throws(() => functions.add_i32(1, 2, 3),
                  { code: 'ERR_INVALID_ARG_VALUE' });
  } finally { lib.close(); }
});

test('5 GP args (SysV register limit)', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_5: { result: 'i32', parameters: ['i32', 'i32', 'i32', 'i32', 'i32'] },
  });
  try {
    assert.strictEqual(functions.add_5(1, 2, 3, 4, 5), 15);
  } finally { lib.close(); }
});

test('6 GP args (SysV stack overflow boundary)', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_6: { result: 'i32', parameters: ['i32', 'i32', 'i32', 'i32', 'i32', 'i32'] },
  });
  try {
    assert.strictEqual(functions.add_6(1, 2, 3, 4, 5, 6), 21);
  } finally { lib.close(); }
});

test('7 GP args (AArch64 boundary)', () => {
  const { lib, functions } = ffi.dlopen(libraryPath, {
    add_7: { result: 'i32', parameters: ['i32', 'i32', 'i32', 'i32', 'i32', 'i32', 'i32'] },
  });
  try {
    assert.strictEqual(functions.add_7(1, 2, 3, 4, 5, 6, 7), 28);
  } finally { lib.close(); }
});

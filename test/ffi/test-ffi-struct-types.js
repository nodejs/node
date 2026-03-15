// Flags: --experimental-ffi
'use strict';
const common = require('../common');
const { describe, it } = require('node:test');
const assert = require('node:assert');

common.skipIfFFIMissing();

const { UnsafeFnPointer, UnsafePointer } = require('node:ffi');

describe('FFI struct descriptor validation', () => {
  it('rejects invalid struct descriptor shape', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      new UnsafeFnPointer(ptr, {
        parameters: [{ notStruct: ['i32'] }],
        result: 'void',
      });
    }, {
      name: 'TypeError',
      message: /Struct descriptor must be an object with a 'struct' array/,
    });
  });

  it('rejects empty struct descriptor', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      new UnsafeFnPointer(ptr, {
        parameters: [{ struct: [] }],
        result: 'void',
      });
    }, {
      name: 'TypeError',
      message: /Struct descriptor must contain at least one field/,
    });
  });

  it('accepts nested struct descriptors', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const fn = new UnsafeFnPointer(ptr, {
      parameters: [{ struct: ['i32', { struct: ['u8', 'u8'] }] }],
      result: 'void',
    });
    assert.strictEqual(typeof fn, 'function');
  });
});

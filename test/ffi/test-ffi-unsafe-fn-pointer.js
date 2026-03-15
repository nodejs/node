// Flags: --experimental-ffi
'use strict';
const { skipIfFFIMissing } = require('../common');
skipIfFFIMissing();
const assert = require('node:assert');
const { describe, it } = require('node:test');
const { dlopen, UnsafePointer, UnsafeFnPointer } = require('node:ffi');

describe('UnsafeFnPointer constructor', () => {
  it('should throw TypeError when called without arguments', () => {
    assert.throws(() => {
      new UnsafeFnPointer();
    }, {
      name: 'TypeError',
      message: 'Expected pointer and definition arguments',
    });
  });

  it('should throw TypeError when called with only pointer', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      new UnsafeFnPointer(ptr);
    }, {
      name: 'TypeError',
      message: 'Expected pointer and definition arguments',
    });
  });

  it('should throw TypeError when pointer is not a PointerObject or BigInt', () => {
    assert.throws(() => {
      new UnsafeFnPointer('not a pointer', {
        parameters: [],
        result: 'void',
      });
    }, {
      name: 'TypeError',
      message: 'First argument must be a PointerObject or BigInt',
    });
  });

  it('should throw TypeError when definition is not an object', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      new UnsafeFnPointer(ptr, 'not an object');
    }, {
      name: 'TypeError',
      message: 'Second argument must be a definition object',
    });
  });

  it('should throw TypeError when definition.parameters is missing', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      new UnsafeFnPointer(ptr, {
        result: 'void',
      });
    }, {
      name: 'TypeError',
      message: 'definition.parameters must be an array',
    });
  });

  it('should throw TypeError when definition.parameters is not an array', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      new UnsafeFnPointer(ptr, {
        parameters: 'not an array',
        result: 'void',
      });
    }, {
      name: 'TypeError',
      message: 'definition.parameters must be an array',
    });
  });

  it('should throw TypeError when definition.parameters contains invalid entries', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      new UnsafeFnPointer(ptr, {
        parameters: ['i32', 42, 'u32'],
        result: 'void',
      });
    }, {
      name: 'TypeError',
      message: 'FFI type must be a string or struct descriptor',
    });
  });

  it('should throw TypeError when definition.result is missing', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      new UnsafeFnPointer(ptr, {
        parameters: [],
      });
    }, {
      name: 'TypeError',
      message: 'FFI type must be a string or struct descriptor',
    });
  });

  it('should throw TypeError when definition.result is not a valid type descriptor', () => {
    const ptr = UnsafePointer.create(0x1000n);
    assert.throws(() => {
      new UnsafeFnPointer(ptr, {
        parameters: [],
        result: 42,
      });
    }, {
      name: 'TypeError',
      message: 'FFI type must be a string or struct descriptor',
    });
  });

  it('should accept PointerObject with valid definition', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const fn = new UnsafeFnPointer(ptr, {
      parameters: ['i32'],
      result: 'i32',
    });
    assert.ok(fn instanceof UnsafeFnPointer);
  });

  it('should accept BigInt with valid definition', () => {
    const fn = new UnsafeFnPointer(0x1000n, {
      parameters: ['i32'],
      result: 'i32',
    });
    assert.ok(fn instanceof UnsafeFnPointer);
  });

  it('should accept empty parameters array', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const fn = new UnsafeFnPointer(ptr, {
      parameters: [],
      result: 'void',
    });
    assert.ok(fn instanceof UnsafeFnPointer);
  });

  it('should accept nonblocking option', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const fn = new UnsafeFnPointer(ptr, {
      parameters: ['i32'],
      result: 'i32',
      nonblocking: true,
    });
    assert.ok(fn instanceof UnsafeFnPointer);
  });

  it('should accept definition with multiple parameter types', () => {
    const ptr = UnsafePointer.create(0x1000n);
    const fn = new UnsafeFnPointer(ptr, {
      parameters: ['i8', 'u8', 'i16', 'u16', 'i32', 'u32', 'i64', 'u64', 'f32', 'f64', 'pointer'],
      result: 'void',
    });
    assert.ok(fn instanceof UnsafeFnPointer);
  });
});

describe('UnsafeFnPointer with real library', () => {
  const libPath = process.platform === 'win32' ? 'msvcrt.dll' :
    process.platform === 'darwin' ? 'libc.dylib' :
      'libc.so.6';

  it('should work with getSymbol and proper definition', () => {
    const lib = dlopen(libPath, {});
    const absSymbol = lib.getSymbol('abs');

    const abs = new UnsafeFnPointer(absSymbol, {
      parameters: ['i32'],
      result: 'i32',
    });

    const result = abs(-42);
    assert.strictEqual(result, 42);
  });

  it('should work with pointer type parameters', () => {
    const lib = dlopen(libPath, {});
    const strlenSymbol = lib.getSymbol('strlen');

    const strlen = new UnsafeFnPointer(strlenSymbol, {
      parameters: ['pointer'],
      result: 'u64',
    });

    const str = new TextEncoder().encode('hello\0');
    const len = strlen(str);
    assert.strictEqual(len, 5n);
  });

  it('should create callable function', () => {
    const lib = dlopen(libPath, {});
    const absSymbol = lib.getSymbol('abs');

    const abs = new UnsafeFnPointer(absSymbol, {
      parameters: ['i32'],
      result: 'i32',
    });

    assert.strictEqual(typeof abs, 'function');
    assert.strictEqual(abs(123), 123);
    assert.strictEqual(abs(-456), 456);
  });
});

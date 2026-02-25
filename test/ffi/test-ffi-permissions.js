// Flags: --permission --experimental-ffi --allow-fs-read=*
'use strict';
const { skipIfFFIMissing } = require('../common');
skipIfFFIMissing();

const test = require('node:test');
const {
  dlopen,
  UnsafePointer,
  UnsafeFnPointer,
  UnsafeCallback,
  UnsafePointerView,
} = require('node:ffi');

test('permission.has("ffi") should be false without --allow-ffi', (t) => {
  t.assert.strictEqual(process.permission.has('ffi'), false);
});

test('ffi.dlopen() should throw ERR_ACCESS_DENIED', (t) => {
  t.assert.throws(() => {
    dlopen('/usr/lib/libc.dylib', {});
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
});

test('UnsafeFnPointer constructor should throw ERR_ACCESS_DENIED', (t) => {
  t.assert.throws(() => {
    new UnsafeFnPointer(0n, {
      returnType: 'int',
      paramTypes: [],
    });
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
});

test('UnsafeCallback constructor should throw ERR_ACCESS_DENIED', (t) => {
  t.assert.throws(() => {
    new UnsafeCallback({
      returnType: 'void',
      paramTypes: [],
    }, () => {});
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
});

test('UnsafePointer constructor should throw ERR_ACCESS_DENIED', (t) => {
  t.assert.throws(() => {
    new UnsafePointer(0n);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
});

test('UnsafePointer.create() should throw ERR_ACCESS_DENIED', (t) => {
  t.assert.throws(() => {
    UnsafePointer.create(0n);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
});

test('UnsafePointerView constructor should throw ERR_ACCESS_DENIED', (t) => {
  t.assert.throws(() => {
    new UnsafePointerView(0n);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
});

test('UnsafePointerView.getCString() should throw ERR_ACCESS_DENIED', (t) => {
  t.assert.throws(() => {
    UnsafePointerView.getCString(0n);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
});

test('UnsafePointerView.copyInto() should throw ERR_ACCESS_DENIED', (t) => {
  const buffer = Buffer.alloc(16);
  t.assert.throws(() => {
    UnsafePointerView.copyInto(0n, buffer);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
});

test('UnsafePointerView.getArrayBuffer() should throw ERR_ACCESS_DENIED', (t) => {
  t.assert.throws(() => {
    UnsafePointerView.getArrayBuffer(0n, 16);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
});

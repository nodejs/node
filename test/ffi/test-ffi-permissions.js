// Flags: --permission --experimental-ffi --allow-fs-read=*
'use strict';
const { skipIfFFIMissing } = require('../common');
skipIfFFIMissing();

const test = require('node:test');
const ffi = require('node:ffi');
const { libraryPath } = require('./ffi-test-common');

test('permission.has("ffi") should be false without --allow-ffi', (t) => {
  t.assert.strictEqual(process.permission.has('ffi'), false);
});

test('dlopen() is denied without permission', (t) => {
  t.assert.throws(() => {
    ffi.dlopen(libraryPath, {});
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
  });
});

test('dlclose() is denied without permission', (t) => {
  t.assert.throws(() => {
    ffi.dlclose({ close() {} });
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
  });
});

test('DynamicLibrary() is denied without permission', (t) => {
  t.assert.throws(() => {
    new ffi.DynamicLibrary(libraryPath);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
  });
});

test('toBuffer() is denied without permission', (t) => {
  t.assert.throws(() => {
    ffi.toBuffer(1n, 4);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
  });
});

test('toArrayBuffer() is denied without permission', (t) => {
  t.assert.throws(() => {
    ffi.toArrayBuffer(1n, 4);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
  });
});

test('getInt32() is denied without permission', (t) => {
  t.assert.throws(() => {
    ffi.getInt32(1n);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
  });
});

test('setInt32() is denied without permission', (t) => {
  t.assert.throws(() => {
    ffi.setInt32(1n, 0, 1);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
  });
});

test('exportString() with explicit size is denied without permission', (t) => {
  t.assert.throws(() => {
    ffi.exportString('hello', 1n, 8);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
  });
});

test('exportString() with auto size is denied without permission', (t) => {
  t.assert.throws(() => {
    ffi.exportString('hello', 1n, 0);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
  });
});

test('exportBuffer() is denied without permission', (t) => {
  t.assert.throws(() => {
    ffi.exportBuffer(Buffer.alloc(0), 1n, 0);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
  });
});

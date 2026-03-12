// Flags: --permission --experimental-ffi --allow-fs-read=*
'use strict';
const common = require('../common');
const {
  strictEqual,
  throws,
} = require('node:assert');
const ffi = require('node:ffi');
const { libraryPath } = require('./ffi-test-common');

common.skipIfFFIMissing();

{
  strictEqual(process.permission.has('ffi'), false);
}

{
  throws(() => {
    ffi.dlopen(libraryPath, {});
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });

  throws(() => {
    new ffi.DynamicLibrary(libraryPath);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
}

{
  throws(() => {
    ffi.toBuffer(1n, 4);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });

  throws(() => {
    ffi.toArrayBuffer(1n, 4);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });

  throws(() => {
    ffi.getInt32(1n);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });

  throws(() => {
    ffi.setInt32(1n, 0, 1);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });

  throws(() => {
    ffi.exportString('hello', 1n, 8);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });

  throws(() => {
    ffi.exportString('hello', 1n, 0);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });

  throws(() => {
    ffi.exportBuffer(Buffer.alloc(0), 1n, 0);
  }, {
    code: 'ERR_ACCESS_DENIED',
    permission: 'FFI',
    message: /Access to this API has been restricted/,
  });
}

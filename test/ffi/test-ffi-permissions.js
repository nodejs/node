// Flags: --permission --experimental-ffi --allow-fs-read=*
'use strict';
const common = require('../common');
const assert = require('node:assert');
const { test } = require('node:test');

common.skipIfFFIMissing();

const ffi = require('node:ffi');

const dummyLibraryPath = 'ffi-permission-test-library';
const denied = {
  code: 'ERR_ACCESS_DENIED',
  permission: 'FFI',
  message: /Access to this API has been restricted\. Use --allow-ffi to manage permissions\./,
};

test('process.permission reports ffi denied by default', () => {
  assert.strictEqual(process.permission.has('ffi'), false);
});

test('permission model blocks dynamic library access', () => {
  assert.throws(() => {
    ffi.dlopen(dummyLibraryPath, {});
  }, denied);

  assert.throws(() => {
    new ffi.DynamicLibrary(dummyLibraryPath);
  }, denied);
});

test('permission model blocks ffi memory and helper APIs', () => {
  assert.throws(() => {
    ffi.toBuffer(1n, 4);
  }, denied);

  assert.throws(() => {
    ffi.toArrayBuffer(1n, 4);
  }, denied);

  assert.throws(() => {
    ffi.getInt32(1n);
  }, denied);

  assert.throws(() => {
    ffi.setInt32(1n, 0, 1);
  }, denied);

  assert.throws(() => {
    ffi.exportString('hello', 1n, 8);
  }, denied);

  assert.throws(() => {
    ffi.exportString('hello', 1n, 0);
  }, denied);

  assert.throws(() => {
    ffi.exportBuffer(Buffer.alloc(0), 1n, 0);
  }, denied);

  assert.throws(() => {
    ffi.dlclose({ close() {} });
  }, denied);
});

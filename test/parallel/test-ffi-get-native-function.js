'use strict';
require('../common');
const assert = require('assert');
const ffi = require('node:ffi');

const getpid = ffi.getNativeFunction(null, 'uv_os_getpid', 'int', []);
assert.strictEqual(process.pid, getpid());

assert.throws(() => {
  ffi.getNativeFunction('strongbad', 'uv_os_getpid', 'int', []);
}, { code: 'ERR_FFI_LIBRARY_LOAD_FAILED' });

assert.throws(() => {
  ffi.getNativeFunction(12345, 'uv_os_getpid', 'int', []);
}, { code: 'ERR_INVALID_ARG_TYPE' });

assert.throws(() => {
  ffi.getNativeFunction(null, 'strongbad', 'int', []);
}, { code: 'ERR_FFI_SYMBOL_NOT_FOUND' });

assert.throws(() => {
  ffi.getNativeFunction(null, 'uv_os_getpid', 'strongbad', []);
}, { code: 'ERR_FFI_UNSUPPORTED_TYPE' });

assert.throws(() => {
  ffi.getNativeFunction(null, 'uv_os_getpid', 'int', ['strongbad']);
}, { code: 'ERR_FFI_UNSUPPORTED_TYPE' });

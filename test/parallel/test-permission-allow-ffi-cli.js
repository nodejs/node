// Flags: --permission --allow-ffi --experimental-ffi --allow-fs-read=*
'use strict';

const { skipIfFFIMissing } = require('../common');
const assert = require('node:assert');
const { fixtureSymbols, libraryPath } = require('../ffi/ffi-test-common');

skipIfFFIMissing();

const ffi = require('node:ffi');

assert.ok(process.permission.has('ffi'));
assert.strictEqual(typeof ffi.DynamicLibrary, 'function');
assert.strictEqual(ffi.toString(0n), null);

const { lib, functions } = ffi.dlopen(libraryPath, {
  add_i32: fixtureSymbols.add_i32,
});

assert.strictEqual(functions.add_i32(19, 23), 42);
assert.strictEqual(ffi.dlsym(lib, 'add_i32'), functions.add_i32.pointer);

lib.close();

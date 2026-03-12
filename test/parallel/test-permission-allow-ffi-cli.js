// Flags: --permission --allow-ffi --experimental-ffi --allow-fs-read=*
'use strict';

const { skipIfFFIMissing } = require('../common');
const assert = require('node:assert');

skipIfFFIMissing();

const ffi = require('node:ffi');

assert.ok(process.permission.has('ffi'));
assert.strictEqual(typeof ffi.DynamicLibrary, 'function');
assert.strictEqual(ffi.toString(0n), null);

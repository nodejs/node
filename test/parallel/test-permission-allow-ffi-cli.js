// Flags: --permission --allow-ffi --experimental-ffi --allow-fs-read=*
'use strict';

const { skipIfFFIMissing } = require('../common');
const assert = require('node:assert');

skipIfFFIMissing();

const ffi = require('node:ffi');

assert.ok(process.permission.has('ffi'));
ffi.UnsafePointer.create(0n);

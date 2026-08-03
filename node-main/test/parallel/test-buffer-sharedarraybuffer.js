'use strict';

require('../common');
const assert = require('assert');

const sab = new SharedArrayBuffer(24);
const arr1 = new Uint16Array(sab);
const arr2 = new Uint16Array(12);
arr2[0] = 5000;
arr1[0] = 5000;
arr1[1] = 4000;
arr2[1] = 4000;

const arr_buf = Buffer.from(arr1.buffer);
const ar_buf = Buffer.from(arr2.buffer);

assert.deepStrictEqual(arr_buf, ar_buf);

arr1[1] = 6000;
arr2[1] = 6000;

assert.deepStrictEqual(arr_buf, ar_buf);

// Checks for calling Buffer.byteLength on a SharedArrayBuffer.
assert.strictEqual(Buffer.byteLength(sab), sab.byteLength);

Buffer.from({ buffer: sab }); // Should not throw.

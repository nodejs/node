/*global SharedArrayBuffer*/
'use strict';
// Flags: --harmony-sharedarraybuffer

require('../common');
const assert = require('assert');
const Buffer = require('buffer').Buffer;

const sab = new SharedArrayBuffer(24);
const arr = new Uint16Array(sab);
const ar = new Uint16Array(12);
ar[0] = 5000;
arr[0] = 5000;
arr[1] = 4000;
ar[1] = 4000;

var arr_buf = Buffer.from(arr.buffer);
var ar_buf = Buffer.from(ar.buffer);

assert.deepStrictEqual(arr_buf, ar_buf, 0);

arr[1] = 6000;
ar[1] = 6000;

assert.deepStrictEqual(arr_buf, ar_buf, 0);

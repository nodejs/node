// Flags: --no-warnings
'use strict';
require('../common');

const assert = require('assert');
const encoding = require('node:encoding');
const { TextEncoder } = require('util');

const encoder = new TextEncoder();

assert.deepStrictEqual(encoding.isAscii(encoder.encode('hello')), true);
assert.deepStrictEqual(encoding.isAscii(encoder.encode('ğ')), false);
assert.deepStrictEqual(encoding.isAscii('hello'), true);
assert.deepStrictEqual(encoding.isAscii('ğ'), false);

assert.deepStrictEqual(encoding.isUtf8(encoder.encode('hello')), true);
assert.deepStrictEqual(encoding.isUtf8(encoder.encode('ğ')), true);
assert.deepStrictEqual(encoding.isUtf8(Buffer.from([0xf8])), false);

assert.deepStrictEqual(encoding.countUtf8(encoder.encode('hello')), 5);
assert.deepStrictEqual(encoding.countUtf8(encoder.encode('Yağız')), 5);

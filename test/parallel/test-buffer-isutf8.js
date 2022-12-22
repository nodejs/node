'use strict';

require('../common');
const assert = require('assert');
const { isUtf8 } = require('buffer');
const { TextEncoder } = require('util');

const encoder = new TextEncoder();

assert.strictEqual(isUtf8(encoder.encode('hello')), true);
assert.strictEqual(isUtf8(encoder.encode('ğ')), true);
assert.strictEqual(isUtf8(Buffer.from([0xf8])), false);

assert.strictEqual(isUtf8(null), false);
assert.strictEqual(isUtf8(undefined), false);

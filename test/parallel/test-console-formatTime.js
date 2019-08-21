'use strict';
// Flags: --expose-internals
require('../common');
const { formatTime } = require('internal/console/constructor');
const assert = require('assert');

const test1 = formatTime(100);
const test2 = formatTime(1500);
const test3 = formatTime(60300);
const test4 = formatTime(4000000);

assert.strictEqual(test1, '100.000ms');
assert.strictEqual(test2, '1.500s');
assert.strictEqual(test3, '1.005min');
assert.strictEqual(test4, '1.111h');

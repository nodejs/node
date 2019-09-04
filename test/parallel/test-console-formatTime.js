'use strict';
// Flags: --expose-internals
require('../common');
const { formatTime } = require('internal/console/constructor');
const assert = require('assert');

assert.strictEqual(formatTime(100.0096), '100.01ms');
assert.strictEqual(formatTime(100.0115), '100.011ms');
assert.strictEqual(formatTime(1500.04), '1s and 500ms');
assert.strictEqual(formatTime(1500.056), '1s and 500.1ms');
assert.strictEqual(formatTime(60300.3), '1min and 300ms');
assert.strictEqual(formatTime(4000457.4), '1h, 6min and 40.5s');
assert.strictEqual(formatTime(3601017.4), '1h and 1s');

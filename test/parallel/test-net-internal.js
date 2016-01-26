'use strict';

// Flags: --expose-internals

require('../common');
const assert = require('assert');
const net = require('internal/net');

assert.strictEqual(net.isLegalPort(''), false);
assert.strictEqual(net.isLegalPort('0'), true);
assert.strictEqual(net.isLegalPort(0), true);
assert.strictEqual(net.isLegalPort(65536), false);
assert.strictEqual(net.isLegalPort('65535'), true);
assert.strictEqual(net.isLegalPort(undefined), false);
assert.strictEqual(net.isLegalPort(null), true);

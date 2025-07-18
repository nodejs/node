'use strict';

require('../common');

const assert = require('assert');

const buffer = Buffer.from([0x12, 0x34, 0x56, 0x78]);

buffer.getInt16BE();
buffer.getInt16BE();

buffer.reset();

assert.strictEqual(buffer.getInt16BE(), 0x1234);
assert.strictEqual(buffer.getInt16BE(), 0x5678);

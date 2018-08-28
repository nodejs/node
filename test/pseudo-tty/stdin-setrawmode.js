'use strict';
require('../common');
require('../common/common-tty');
const assert = require('assert');

process.stdin.setRawMode(true);
assert.strictEqual(process.stdin.isRaw, true);

process.stdin.setRawMode(false);
assert.strictEqual(process.stdin.isRaw, false);

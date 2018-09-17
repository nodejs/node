'use strict';
require('../common');
const assert = require('assert');

const origNextTick = process.nextTick;

require('domain');

// Requiring domain should not change nextTick.
assert.strictEqual(origNextTick, process.nextTick);

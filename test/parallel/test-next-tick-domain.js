'use strict';
require('../common');
const assert = require('assert');

const origNextTick = process.nextTick;

require('domain');

assert.strictEqual(origNextTick, process.nextTick,
                   'Requiring domain should not change nextTick');

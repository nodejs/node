'use strict';
const common = require('../common');
const assert = require('assert');

var origNextTick = process.nextTick;

require('domain');

assert.strictEqual(origNextTick, process.nextTick,
                   'Requiring domain should not change nextTick');

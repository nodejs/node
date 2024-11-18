'use strict';

const common = require('../common');
const assert = require('assert');

common.allowGlobals('rawDebug');
assert.strictEqual(typeof global.rawDebug === 'function', true);

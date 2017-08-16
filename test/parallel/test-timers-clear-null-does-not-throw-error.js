'use strict';
require('../common');
const assert = require('assert');

// This test makes sure clearing timers with
// 'null' or no input does not throw error

assert.doesNotThrow(() => clearInterval(null));

assert.doesNotThrow(() => clearInterval());

assert.doesNotThrow(() => clearTimeout(null));

assert.doesNotThrow(() => clearTimeout());

assert.doesNotThrow(() => clearImmediate(null));

assert.doesNotThrow(() => clearImmediate());

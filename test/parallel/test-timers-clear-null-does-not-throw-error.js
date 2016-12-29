'use strict';
require('../common');
const assert = require('assert');

// This test makes sure clearing timers with
// 'null' or no input does not throw error

assert.doesNotThrow(() => { return clearInterval(null); });

assert.doesNotThrow(() => { return clearInterval(); });

assert.doesNotThrow(() => { return clearTimeout(null); });

assert.doesNotThrow(() => { return clearTimeout(); });

assert.doesNotThrow(() => { return clearImmediate(null); });

assert.doesNotThrow(() => { return clearImmediate(); });

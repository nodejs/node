'use strict';

require('../common');
const assert = require('assert');

// https://github.com/nodejs/node/issues/21219
assert.strictEqual(Atomics.wake, undefined);

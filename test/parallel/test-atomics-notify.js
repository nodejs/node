'use strict';

require('../common');

const assert = require('assert');
const { runInNewContext } = require('vm');

assert.strictEqual(Atomics.wake, Atomics.notify);

assert(runInNewContext('Atomics.wake === Atomics.notify'));

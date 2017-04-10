'use strict';

require('../common');
const assert = require('assert');

const timer1 = setTimeout(() => {}, 1).unref();
assert.strictEqual(timer1._handle._list, undefined,
                   'timer1._handle._list should be undefined');

// Check that everything works even if the handle was not re-used.
setTimeout(() => {}, 1);
const timer2 = setTimeout(() => {}, 1).unref();
// Note: It is unlikely that this assertion will be hit in a failure.
// Instead, timers.js will likely throw a TypeError internally.
assert.strictEqual(timer2._handle._list, undefined,
                   'timer2._handle._list should be undefined');

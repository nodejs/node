'use strict';

// Should be above require, because code in require read console
// what we are trying to avoid
// set should be earlier than get

global.console = undefined;

// Initially, the `console` variable is `undefined`, since console will be
// lazily loaded in the getter.

require('../common');
const assert = require('assert');

// global.console's getter is called
// Since the `console` cache variable is `undefined` and therefore false-y,
// the getter still calls NativeModule.require() and returns the object
// obtained from it, instead of returning `undefined` as expected.

assert.strictEqual(global.console, undefined, 'first read');
assert.strictEqual(global.console, undefined, 'second read');

global.console = 1;
assert.strictEqual(global.console, 1, 'set true-like primitive');

global.console = 0;
assert.strictEqual(global.console, 0, 'set false-like primitive, again');

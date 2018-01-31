'use strict';

// Patch global.console before importing modules that may modify the console
// object.

const tmp = global.console;
global.console = 42;

const common = require('../common');
const assert = require('assert');

// Originally the console had a getter. Test twice to verify it had no side
// effect.
assert.strictEqual(global.console, 42);
assert.strictEqual(global.console, 42);

common.expectsError(
  () => console.log('foo'),
  {
    type: TypeError,
    message: 'console.log is not a function'
  }
);

global.console = 1;
assert.strictEqual(global.console, 1);
assert.strictEqual(console, 1);

// Reset the console
global.console = tmp;
console.log('foo');

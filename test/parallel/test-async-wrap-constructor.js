'use strict';
require('../common');

// This tests that using falsy values in createHook throws an error.

const assert = require('assert');
const async_hooks = require('async_hooks');

for (const badArg of [0, 1, false, true, null, 'hello']) {
  for (const field of ['init', 'before', 'after', 'destroy']) {
    assert.throws(() => {
      async_hooks.createHook({ [field]: badArg });
    }, new RegExp(`^TypeError: ${field} must be a function$`));
  }
}

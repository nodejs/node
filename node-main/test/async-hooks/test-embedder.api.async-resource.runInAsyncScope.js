'use strict';
require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

// Ensure that asyncResource.makeCallback returns the callback return value.
const a = new async_hooks.AsyncResource('foobar');
const ret = a.runInAsyncScope(() => {
  return 1729;
});
assert.strictEqual(ret, 1729);

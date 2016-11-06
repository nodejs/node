'use strict';

// Test for bug where a timer duration greater than 0 ms but less than 1 ms
// resulted in the duration being set for 1000 ms. The expected behavior is
// that the timeout would be set for 1 ms, and thus fire more-or-less
// immediately.
//
// Ref: https://github.com/nodejs/node-v0.x-archive/pull/897

const common = require('../common');
const assert = require('assert');

const t = Date.now();
setTimeout(common.mustCall(function() {
  const diff = Date.now() - t;
  assert.ok(diff < 100, `timer fired after ${diff} ms`);
}), 0.1);

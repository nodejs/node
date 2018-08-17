'use strict';

const common = require('../common');
const assert = require('assert');

// This test checks that throwing inside a setTimeout where that Timeout
// instance is the only one within its list of timeouts, doesn't cause
// an issue with an unref timeout scheduled in the error handler.
// Refs: https://github.com/nodejs/node/issues/19970

const timeout = common.platformTimeout(50);

const timer = setTimeout(common.mustNotCall(), 2 ** 31 - 1);

process.once('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'setTimeout Error');

  const now = Date.now();
  setTimeout(common.mustCall(() => {
    assert(Date.now() - now >= timeout);
    clearTimeout(timer);
  }), timeout).unref();
}));

setTimeout(() => {
  throw new Error('setTimeout Error');
}, timeout);

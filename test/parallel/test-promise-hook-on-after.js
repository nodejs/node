'use strict';
const common = require('../common');
const assert = require('assert');
const { onAfter } = require('promise_hooks');

let seen;

const stop = onAfter(common.mustCall((promise) => {
  seen = promise;
}, 1));

const promise = Promise.resolve().then(() => {
  assert.strictEqual(seen, undefined);
});

promise.then(() => {
  assert.strictEqual(seen, promise);
  stop();
});

assert.strictEqual(seen, undefined);

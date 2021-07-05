'use strict';
const common = require('../common');
const assert = require('assert');
const { onBefore } = require('promise_hooks');

let seen;

const stop = onBefore(common.mustCall((promise) => {
  seen = promise;
}, 1));

const promise = Promise.resolve().then(() => {
  assert.strictEqual(seen, promise);
  stop();
});

promise.then();

assert.strictEqual(seen, undefined);

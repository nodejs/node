'use strict';
const common = require('../common');
const assert = require('assert');
const { createHook } = require('promise_hooks');

let init;
let initParent;
let before;
let after;
let resolve;

const stop = createHook({
  init: common.mustCall((promise, parent) => {
    init = promise;
    initParent = parent;
  }, 3),
  before: common.mustCall((promise) => {
    before = promise;
  }, 2),
  after: common.mustCall((promise) => {
    after = promise;
  }, 1),
  resolve: common.mustCall((promise) => {
    resolve = promise;
  }, 2)
});

// Clears state on each check so only the delta needs to be checked.
function assertState(expectedInit, expectedInitParent, expectedBefore,
                     expectedAfter, expectedResolve) {
  assert.strictEqual(init, expectedInit);
  assert.strictEqual(initParent, expectedInitParent);
  assert.strictEqual(before, expectedBefore);
  assert.strictEqual(after, expectedAfter);
  assert.strictEqual(resolve, expectedResolve);
  init = undefined;
  initParent = undefined;
  before = undefined;
  after = undefined;
  resolve = undefined;
}

const parent = Promise.resolve(1);
// After calling `Promise.resolve(...)`, the returned promise should have
// produced an init event with no parent and a resolve event.
assertState(parent, undefined, undefined, undefined, parent);

const child = parent.then(() => {
  // When a callback to `promise.then(...)` is called, the promise it resolves
  // to should have produced a before event to mark the start of this callback.
  assertState(undefined, undefined, child, undefined, undefined);
});
// After calling `promise.then(...)`, the returned promise should have
// produced an init event with a parent of the promise the `then(...)`
// was called on.
assertState(child, parent);

const grandChild = child.then(() => {
  // Since the check for the before event in the `then(...)` call producing the
  // `child` promise, there should have been both a before event for this
  // promise but also resolve and after events for the `child` promise.
  assertState(undefined, undefined, grandChild, child, child);
  stop();
});
// After calling `promise.then(...)`, the returned promise should have
// produced an init event with a parent of the promise the `then(...)`
// was called on.
assertState(grandChild, child);

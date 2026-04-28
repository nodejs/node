'use strict';
const common = require('../common');
const assert = require('assert');
const { promiseHooks } = require('v8');

for (const hook of ['init', 'before', 'after', 'settled']) {
  assert.throws(() => {
    promiseHooks.createHook({
      [hook]: async function() { }
    });
  }, new RegExp(`The "${hook}Hook" argument must be of type function`));

  assert.throws(() => {
    promiseHooks.createHook({
      [hook]: async function*() { }
    });
  }, new RegExp(`The "${hook}Hook" argument must be of type function`));
}

let init;
let initParent;
let before;
let after;
let settled;

const stop = promiseHooks.createHook({
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
  settled: common.mustCall((promise) => {
    settled = promise;
  }, 2)
});

// Clears state on each check so only the delta needs to be checked.
function assertState(expectedInit, expectedInitParent, expectedBefore,
                     expectedAfter, expectedSettled) {
  assert.strictEqual(init, expectedInit);
  assert.strictEqual(initParent, expectedInitParent);
  assert.strictEqual(before, expectedBefore);
  assert.strictEqual(after, expectedAfter);
  assert.strictEqual(settled, expectedSettled);
  init = undefined;
  initParent = undefined;
  before = undefined;
  after = undefined;
  settled = undefined;
}

const parent = Promise.resolve(1);
// After calling `Promise.resolve(...)`, the returned promise should have
// produced an init event with no parent and a settled event.
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
  // promise but also settled and after events for the `child` promise.
  assertState(undefined, undefined, grandChild, child, child);
  stop();
});
// After calling `promise.then(...)`, the returned promise should have
// produced an init event with a parent of the promise the `then(...)`
// was called on.
assertState(grandChild, child);

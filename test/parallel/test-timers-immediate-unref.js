'use strict';

const common = require('../common');
const Countdown = require('../common/countdown');

const assert = require('assert');

const immediate = setImmediate(() => {});
assert.strictEqual(immediate.hasRef(), true);
immediate.unref();
assert.strictEqual(immediate.hasRef(), false);
clearImmediate(immediate);

// This immediate should execute as it was unrefed and refed again.
// It also confirms that unref/ref are chainable.
setImmediate(common.mustCall(firstStep)).ref().unref().unref().ref();

function firstStep() {
  const countdown =
    new Countdown(2, common.mustCall(() => setImmediate(secondStep)));
  // Unrefed setImmediate executes if it was unrefed but something else keeps
  // the loop open
  setImmediate(() => countdown.dec()).unref();
  setTimeout(() => countdown.dec(), 50);
}

function secondStep() {
  // clearImmediate works just fine with unref'd immediates
  const immA = setImmediate(() => {
    clearImmediate(immA);
    clearImmediate(immB);
    // This should not keep the event loop open indefinitely
    // or do anything else weird
    immA.ref();
    immB.ref();
  }).unref();
  const immB = setImmediate(common.mustNotCall()).unref();
  setImmediate(common.mustCall(finalStep));
}

function finalStep() {
  // This immediate should not execute as it was unrefed
  // and nothing else is keeping the event loop alive
  setImmediate(common.mustNotCall()).unref();
}

'use strict';

const common = require('../common');
const Countdown = require('../common/countdown');

// This immediate should execute as it was unrefed and refed again.
// It also confirms that unref/ref are chainable.
setImmediate(common.mustCall(firstStep)).ref().unref().unref().ref();

function firstStep() {
  const countdown = new Countdown(2, () => setImmediate(finalStep));
  // Unrefed setImmediate executes if it was unrefed but something else keeps
  // the loop open
  setImmediate(common.mustCall(() => countdown.dec())).unref();
  setTimeout(common.mustCall(() => countdown.dec()), 50);
}

function finalStep() {
  // This immediate should not execute as it was unrefed
  // and nothing else is keeping the event loop alive
  setImmediate(common.mustNotCall()).unref();
}

'use strict';
const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

// Checks that enabling async hooks in a callback actually
// triggers after & destroy as expected.

const fnsToTest = [setTimeout, (cb) => {
  setImmediate(() => {
    cb();

    // We need to keep the event loop open for this to actually work
    // since destroy hooks are triggered in unrefed Immediates
    setImmediate(() => {
      hook.disable();
    });
  });
}, (cb) => {
  setImmediate(() => {
    process.nextTick(() => {
      cb();

      // We need to keep the event loop open for this to actually work
      // since destroy hooks are triggered in unrefed Immediates
      setImmediate(() => {
        hook.disable();
        assert.strictEqual(fnsToTest.length, 0);
      });
    });
  });
}];

const hook = async_hooks.createHook({
  before: common.mustNotCall(),
  after: common.mustCall(3),
  destroy: common.mustCall(() => {
    hook.disable();
    nextTest();
  }, 3),
});

nextTest();

function nextTest() {
  if (fnsToTest.length > 0) {
    fnsToTest.shift()(common.mustCall(() => {
      hook.enable();
    }));
  }
}

'use strict';
// Flags: --expose-gc

// The test verifies that finalizers are called as part of the main script
// execution when there are no exceptions.

const common = require('../../common');
const assert = require('assert');
const test = require(`./build/${common.buildType}/test_finalizer_exception`);

assert.strictEqual(test.finalizeCount, 0);

function gcUntilSync(value) {
  for (let i = 0; i < 10; ++i) {
    global.gc();
    if (test.finalizeCount === value) {
      break;
    }
  }
}

function runGCTests() {
  (() => {
    test.createObject();
  })();
  gcUntilSync(1);
  assert.strictEqual(test.finalizeCount, 1);

  (() => {
    test.createObject();
    test.createObject();
  })();
  gcUntilSync(3);
  assert.strictEqual(test.finalizeCount, 3);
}
runGCTests();

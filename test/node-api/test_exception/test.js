'use strict';
// Flags: --expose-gc
// Addons: test_exception, test_exception_vtable

const assert = require('assert');
const { addonPath } = require('../../common/addon-test');
const test_exception = require(addonPath);

// Make sure that exceptions that occur during finalization are propagated.
function testFinalize(binding) {
  let x = test_exception[binding]();
  x = null;
  global.gc();
  process.on('uncaughtException', (err) => {
    // eslint-disable-next-line node-core/must-call-assert
    assert.strictEqual(err.message, 'Error during Finalize');
  });

  // To assuage the linter's concerns.
  (function() {})(x);
}
testFinalize('createExternalBuffer');

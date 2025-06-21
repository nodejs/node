'use strict';
// Flags: --expose-gc

const common = require('../../common');
const assert = require('assert');
const test_exception = require(`./build/${common.buildType}/test_exception`);

// Make sure that exceptions that occur during finalization are propagated.
function testFinalize(binding) {
  let x = test_exception[binding]();
  x = null;
  global.gc();
  process.on('uncaughtException', (err) => {
    assert.strictEqual(err.message, 'Error during Finalize');
  });

  // To assuage the linter's concerns.
  (function() {})(x);
}
testFinalize('createExternalBuffer');

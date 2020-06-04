'use strict';
require('../common');
const assert = require('assert');

// This test ensures that assert.trackCalls() checks proper usage on exit.

const trackedFunc = assert.trackCalls();
trackedFunc();

process.on('uncaughtException', (err) => {
  assert.strictEqual(process._exiting, true);
  assert.strictEqual(
    err.message,
    'The call tracked "trackedFunction" function was not verified before ' +
      'exit.\nIf you want the function to implicitly be verified on exit, ' +
      'set the "verifyOnExit" option to "true"'
  );
  assert.deepStrictEqual({ ...err }, {
    generatedMessage: true,
    code: 'ERR_ASSERTION',
    actual: '.verify() has not been called',
    expected: '.verify() to be called before program exit',
    operator: 'On exit assert.trackCalls check'
  });
});

'use strict';
require('../common');
const assert = require('assert');

// This test ensures that assert.trackCalls() checks proper usage on exit with
// multiple missed verify calls.

const trackedFunc = assert.trackCalls({ failEarly: false });
const foo = () => trackedFunc();
foo();
foo();

assert.trackCalls()();
assert.trackCalls(console.log, { mode: 'minimum' });

const secondFunc = trackedFunc.add();
secondFunc();
assert.throws(
  () => secondFunc(),
  {
    code: 'ERR_ASSERTION',
    message: 'Mismatched trackedFunction function calls. ' +
             'Expected exactly 1, actual 2.'
  }
);

process.on('uncaughtException', (err) => {
  assert.strictEqual(process._exiting, true);
  assert.strictEqual(
    err.stack,
    'AssertionError [ERR_ASSERTION]: Mismatched function calls and missed .ve' +
      'rify() calls detected on exit. Check the "errors" property for details.'
  );
  assert.deepStrictEqual({ ...err, errors: [] }, {
    generatedMessage: true,
    code: 'ERR_ASSERTION',
    actual: '3 functions have not been verified or have a mismatched ' +
            'function call',
    expected: 'All functions to be called the correct number of times',
    operator: 'verifyTracker',
    errors: []
  });

  assert.strictEqual(
    err.errors[0].message,
    'Mismatched trackedFunction function calls. Expected exactly 1, actual 2.'
  );
  assert.deepStrictEqual({ ...err.errors[0], callFrames: null }, {
    generatedMessage: true,
    code: 'ERR_ASSERTION',
    actual: 2,
    expected: 1,
    operator: 'trackedFunction',
    callFrames: null,
  });
  const callFrames = Object.entries(err.errors[0].callFrames);
  assert.strictEqual(callFrames.length, 1);
  assert.match(callFrames[0][0], /[^\s].+test-assert-track-calls/);
  assert.strictEqual(callFrames[0][1], 2);

  assert.strictEqual(
    err.errors[1].message,
    'The call tracked "trackedFunction" function was not verified before ' +
      'exit.\nIf you want the function to implicitly be verified on exit, ' +
      'set the "verifyOnExit" option to "true"'
  );
  assert.deepStrictEqual({ ...err.errors[1] }, {
    generatedMessage: true,
    code: 'ERR_ASSERTION',
    actual: '.verify() has not been called',
    expected: '.verify() to be called before program exit',
    operator: 'On exit assert.trackCalls check'
  });

  assert.strictEqual(
    err.errors[2].message,
    'Mismatched bound consoleCall function calls. ' +
      'Expected at least 1, actual 0.'
  );
  assert.deepStrictEqual({ ...err.errors[2] }, {
    generatedMessage: true,
    code: 'ERR_ASSERTION',
    actual: 0,
    expected: 1,
    operator: 'bound consoleCall',
    callFrames: {},
  });
});

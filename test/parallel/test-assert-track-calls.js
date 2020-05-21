'use strict';
require('../common');
const assert = require('assert');

// This test ensures that assert.trackCalls() works as intended.

function bar() {}

// Ensures calls() throws on invalid input types.
['1', null, true, () => {}, { expected: true }].forEach((invalidExpected) => {
  assert.throws(() => assert.trackCalls(bar, invalidExpected), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});
[0, Infinity, NaN, { expected: 0 }].forEach((invalidExpected) => {
  assert.throws(() => assert.trackCalls(bar, invalidExpected), {
    code: 'ERR_OUT_OF_RANGE',
  });
});
['foobar', 'exactly'].forEach((invalidMode) => {
  assert.throws(() => assert.trackCalls(bar, { mode: invalidMode }), {
    code: 'ERR_INVALID_OPT_VALUE',
  });
});
[1, true, Symbol('foo')].forEach((invalidMode) => {
  assert.throws(() => assert.trackCalls(bar, { mode: invalidMode }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});
[1, [], {}, '1'].forEach((verifyOnExit) => {
  assert.throws(() => assert.trackCalls(bar, { verifyOnExit }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});
[1, [], {}, '1'].forEach((failEarly) => {
  assert.throws(() => assert.trackCalls(bar, { failEarly }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
});
// TODO: Check that 0 explicitly mentions to switch to assert.mustNotCall().
[0, Infinity, NaN, { expected: 0 }].forEach((invalidExpected) => {
  assert.throws(() => assert.trackCalls(bar, invalidExpected), {
    code: 'ERR_OUT_OF_RANGE',
  });
});
[1, { expected: 1 }].forEach((ambiguousFn) => {
  assert.throws(() => assert.trackCalls(ambiguousFn, 1), {
    code: 'ERR_AMBIGUOUS_ARGUMENT',
  });
});

// Check that assert.trackCalls() works properly during process exit.
process.on('exit', () => {
  const trackedFn = assert.trackCalls(bar, {
    verifyOnExit: false
  });
  trackedFn();
  assert.throws(() => trackedFn(), { code: 'ERR_ASSERTION' });

  // This does not check that `verify()` is called.
  assert.trackCalls(bar, {
    verifyOnExit: false
  });

  assert.throws(() => assert.trackCalls(bar, 1), {
    code: 'ERR_UNAVAILABLE_DURING_EXIT',
  });
  assert.throws(() => assert.trackCalls(bar, {
    verifyOnExit: true
  }), {
    code: 'ERR_UNAVAILABLE_DURING_EXIT',
  });
});

// Valid use cases.
{
  const message = 'Expected to throw';

  function throwingFunc() {
    throw new Error(message);
  }

  // Check that throwing functions are removed from the list of tracked ones, if
  // verified.
  let trackedFunc = assert.trackCalls(throwingFunc, 1);
  // Expects trackedFunc() to call func() which throws an error.
  assert.throws(() => trackedFunc(), { message });
  // We have to call .verify(), otherwise this would end up as error.
  trackedFunc.verify();

  // Early failures result in errors. These functions won't be checked on exit.
  trackedFunc = assert.trackCalls();
  assert.throws(() => trackedFunc.verify(), {
    code: 'ERR_ASSERTION',
    message: 'Mismatched trackedFunction function calls. ' +
             'Expected exactly 1, actual 0.'
  });

  // Early failures result in errors. These functions won't be checked on exit.
  trackedFunc = assert.trackCalls(2);
  trackedFunc();
  trackedFunc();
  assert.throws(() => trackedFunc(), {
    code: 'ERR_ASSERTION',
    message: 'Mismatched trackedFunction function calls. ' +
             'Expected exactly 2, actual 3.'
  });

  // Trigger the correct number of times and call .verify().
  trackedFunc = assert.trackCalls(bar, 2);
  trackedFunc();
  trackedFunc();
  trackedFunc.verify();

  // Check minimum mode
  trackedFunc = assert.trackCalls({ expected: 2, mode: 'minimum' });
  trackedFunc();
  trackedFunc();
  trackedFunc();
  trackedFunc.verify();

  // Check failEarly set to false
  trackedFunc = assert.trackCalls(() => {}, { expected: 2, failEarly: false });
  trackedFunc();
  trackedFunc();
  trackedFunc();
  trackedFunc();
  assert.throws(() => trackedFunc.verify(), {
    code: 'ERR_ASSERTION',
    message: 'Mismatched <anonymous call tracked function> function calls. ' +
             'Expected exactly 2, actual 4.'
  });

  // Check that failEarly set to false and verifyOnExit set to false does not
  // result in an error in case `.verify()` was not called.
  trackedFunc = assert.trackCalls(bar, {
    expected: 2,
    failEarly: false,
    verifyOnExit: true
  });
  trackedFunc();
  trackedFunc();
  trackedFunc();

  // Check verifyOnExit set to true does not need a .verify() call.
  trackedFunc = assert.trackCalls(bar, { expected: 2, verifyOnExit: true });
  trackedFunc();
  trackedFunc();
  assert.throws(() => trackedFunc.verify(), {
    code: 'ERR_ASSERTION',
    message: 'NOT IMPLEMENTED'
  });

  // This is not causing any errors, since it's never verified.
  assert.trackCalls({ verifyOnExit: false });
}

assert.trackCalls(console.log, 1);

process.on('uncaughtException', (err) => {
  assert.strictEqual(
    err.stack,
    'AssertionError [ERR_ASSERTION]: Mismatched function calls detected on ' +
      'exit. Check the "errors" property for details.'
  );
  assert.strictEqual(
    err.errors[0].message,
    'Mismatched bar function calls. Expected exactly 2, actual 3.'
  );
  assert.strictEqual(
    err.errors[1].message,
    'Mismatched bound consoleCall function calls. Expected exactly 1, actual 0.'
  );
  assert.deepStrictEqual({ ...err.errors[0], callFrames: null }, {
    generatedMessage: true,
    code: 'ERR_ASSERTION',
    actual: 3,
    expected: 2,
    operator: 'bar',
    callFrames: null,
  });
  const callFrames = Object.entries(err.errors[0].callFrames);
  assert.strictEqual(callFrames.length, 3);
  for (const [frame, calls] of callFrames) {
    assert.match(frame, /[^\s].+test-assert-track-calls/);
    assert.strictEqual(calls, 1);
  }
  assert.deepStrictEqual({ ...err.errors[1] }, {
    generatedMessage: true,
    code: 'ERR_ASSERTION',
    actual: 0,
    expected: 1,
    operator: 'bound consoleCall',
    callFrames: {}
  });
});

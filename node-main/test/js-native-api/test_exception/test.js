'use strict';
// Flags: --expose-gc

const common = require('../../common');
const assert = require('assert');
const theError = new Error('Some error');

// The test module throws an error during Init, but in order for its exports to
// not be lost, it attaches them to the error's "bindings" property. This way,
// we can make sure that exceptions thrown during the module initialization
// phase are propagated through require() into JavaScript.
// https://github.com/nodejs/node/issues/19437
const test_exception = (function() {
  let resultingException;
  try {
    require(`./build/${common.buildType}/test_exception`);
  } catch (anException) {
    resultingException = anException;
  }
  assert.strictEqual(resultingException.message, 'Error during Init');
  return resultingException.binding;
})();

{
  const throwTheError = () => { throw theError; };

  // Test that the native side successfully captures the exception
  let returnedError = test_exception.returnException(throwTheError);
  assert.strictEqual(returnedError, theError);

  // Test that the native side passes the exception through
  assert.throws(
    () => { test_exception.allowException(throwTheError); },
    (err) => err === theError,
  );

  // Test that the exception thrown above was marked as pending
  // before it was handled on the JS side
  const exception_pending = test_exception.wasPending();
  assert.strictEqual(exception_pending, true,
                     'Exception not pending as expected,' +
                     ` .wasPending() returned ${exception_pending}`);

  // Test that the native side does not capture a non-existing exception
  returnedError = test_exception.returnException(common.mustCall());
  assert.strictEqual(returnedError, undefined,
                     'Returned error should be undefined when no exception is' +
                     ` thrown, but ${returnedError} was passed`);
}


{
  const throwTheError = class { constructor() { throw theError; } };

  // Test that the native side successfully captures the exception
  let returnedError = test_exception.constructReturnException(throwTheError);
  assert.strictEqual(returnedError, theError);

  // Test that the native side passes the exception through
  assert.throws(
    () => { test_exception.constructAllowException(throwTheError); },
    (err) => err === theError,
  );

  // Test that the exception thrown above was marked as pending
  // before it was handled on the JS side
  const exception_pending = test_exception.wasPending();
  assert.strictEqual(exception_pending, true,
                     'Exception not pending as expected,' +
                     ` .wasPending() returned ${exception_pending}`);

  // Test that the native side does not capture a non-existing exception
  returnedError = test_exception.constructReturnException(common.mustCall());
  assert.strictEqual(returnedError, undefined,
                     'Returned error should be undefined when no exception is' +
                     ` thrown, but ${returnedError} was passed`);
}

{
  // Test that no exception appears that was not thrown by us
  let caughtError;
  try {
    test_exception.allowException(common.mustCall());
  } catch (anError) {
    caughtError = anError;
  }
  assert.strictEqual(caughtError, undefined,
                     'No exception originated on the native side, but' +
                     ` ${caughtError} was passed`);

  // Test that the exception state remains clear when no exception is thrown
  const exception_pending = test_exception.wasPending();
  assert.strictEqual(exception_pending, false,
                     'Exception state did not remain clear as expected,' +
                     ` .wasPending() returned ${exception_pending}`);
}

{
  // Test that no exception appears that was not thrown by us
  let caughtError;
  try {
    test_exception.constructAllowException(common.mustCall());
  } catch (anError) {
    caughtError = anError;
  }
  assert.strictEqual(caughtError, undefined,
                     'No exception originated on the native side, but' +
                     ` ${caughtError} was passed`);

  // Test that the exception state remains clear when no exception is thrown
  const exception_pending = test_exception.wasPending();
  assert.strictEqual(exception_pending, false,
                     'Exception state did not remain clear as expected,' +
                     ` .wasPending() returned ${exception_pending}`);
}

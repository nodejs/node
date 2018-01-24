'use strict';

const common = require('../../common');
const test_exception = require(`./build/${common.buildType}/test_exception`);
const assert = require('assert');
const theError = new Error('Some error');

{
  const throwTheError = () => { throw theError; };

  // Test that the native side successfully captures the exception
  let returnedError = test_exception.returnException(throwTheError);
  assert.strictEqual(theError, returnedError);

  // Test that the native side passes the exception through
  assert.throws(
    () => { test_exception.allowException(throwTheError); },
    (err) => err === theError
  );

  // Test that the exception thrown above was marked as pending
  // before it was handled on the JS side
  assert.strictEqual(test_exception.wasPending(), true,
                     'VM was marked as having an exception pending' +
                     ' when it was allowed through');

  // Test that the native side does not capture a non-existing exception
  returnedError = test_exception.returnException(common.mustCall());
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
  assert.strictEqual(test_exception.wasPending(), false,
                     'VM was not marked as having an exception pending' +
                     ' when none was allowed through');
}

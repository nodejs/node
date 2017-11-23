'use strict';

const common = require('../../common');

// This tests the promise-related n-api calls

const assert = require('assert');
const test_promise = require(`./build/${common.buildType}/test_promise`);

common.crashOnUnhandledRejection();

// A resolution
{
  const expected_result = 42;
  const promise = test_promise.createPromise();
  promise.then(
    common.mustCall(function(result) {
      assert.strictEqual(result, expected_result);
    }),
    common.mustNotCall());
  test_promise.concludeCurrentPromise(expected_result, true);
}

// A rejection
{
  const expected_result = 'It\'s not you, it\'s me.';
  const promise = test_promise.createPromise();
  promise.then(
    common.mustNotCall(),
    common.mustCall(function(result) {
      assert.strictEqual(result, expected_result);
    }));
  test_promise.concludeCurrentPromise(expected_result, false);
}

// Chaining
{
  const expected_result = 'chained answer';
  const promise = test_promise.createPromise();
  promise.then(
    common.mustCall(function(result) {
      assert.strictEqual(result, expected_result);
    }),
    common.mustNotCall());
  test_promise.concludeCurrentPromise(Promise.resolve('chained answer'), true);
}

assert.strictEqual(test_promise.isPromise(test_promise.createPromise()), true);

const rejectPromise = Promise.reject(-1);
const expected_reason = -1;
assert.strictEqual(test_promise.isPromise(rejectPromise), true);
rejectPromise.catch((reason) => {
  assert.strictEqual(reason, expected_reason);
});

assert.strictEqual(test_promise.isPromise(2.4), false);
assert.strictEqual(test_promise.isPromise('I promise!'), false);
assert.strictEqual(test_promise.isPromise(undefined), false);
assert.strictEqual(test_promise.isPromise(null), false);
assert.strictEqual(test_promise.isPromise({}), false);

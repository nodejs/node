'use strict';

const common = require('../../common');
const test_promise = require(`./build/${common.buildType}/test_promise`);
const assert = require('assert');

let promise;

// A resolution
{
  let expected_result = 42;
  promise = test_promise.createPromise();
  promise.then(
    common.mustCall(function(result) {
      assert.strictEqual(result, expected_result,
                         `promise resolved as expected, received ${result}`);
    }),
    common.mustNotCall());
  test_promise.concludeCurrentPromise(expected_result, true);
}

// A rejection
{
  let expected_result = 'It\'s not you, it\'s me.';
  promise = test_promise.createPromise();
  promise.then(
    common.mustNotCall(),
    common.mustCall(function(result) {
      assert.strictEqual(result, expected_result,
                         `promise rejected as expected, received ${result}`);
    }));
  test_promise.concludeCurrentPromise(expected_result, false);
}

// Chaining
promise = test_promise.createPromise();
promise.then(
  common.mustCall(function(result) {
    assert.strictEqual(result, 'chained answer',
                       'resolving with a promise chains properly');
  }),
  common.mustNotCall());
test_promise.concludeCurrentPromise(Promise.resolve('chained answer'), true);

assert.strictEqual(test_promise.isPromise(promise), true);

assert.strictEqual(test_promise.isPromise(Promise.reject(-1)), true);

assert.strictEqual(test_promise.isPromise(2.4), false);

assert.strictEqual(test_promise.isPromise('I promise!'), false);

assert.strictEqual(test_promise.isPromise(undefined), false);

assert.strictEqual(test_promise.isPromise(null), false);

assert.strictEqual(test_promise.isPromise({}), false);

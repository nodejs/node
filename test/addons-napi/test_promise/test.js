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

let result;
result = test_promise.isPromise(promise);
assert.strictEqual(result, true,
                   'natively created promise is recognized as a promise' +
                   `, received ${result}`);

result = test_promise.isPromise(Promise.reject(-1));
assert.strictEqual(result, true,
                   'Promise created with JS is recognized as a promise' +
                   `, received ${result}`);

result = test_promise.isPromise(2.4);
assert.strictEqual(result, false,
                   'Number is recognized as not a promise' +
                   `, received ${result}`);

result = test_promise.isPromise('I promise!');
assert.strictEqual(result, false,
                   'String is recognized as not a promise' +
                   `, received ${result}`);

result = test_promise.isPromise(undefined);
assert.strictEqual(result, false,
                   'undefined is recognized as not a promise' +
                   `, received ${result}`);

result = test_promise.isPromise(null);
assert.strictEqual(result, false,
                   'null is recognized as not a promise' +
                   `, received ${result}`);

result = test_promise.isPromise({});
assert.strictEqual(result, false,
                   'an object is recognized as not a promise' +
                   `, received ${result}`);

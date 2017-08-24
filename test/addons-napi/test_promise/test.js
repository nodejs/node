'use strict';

const common = require('../../common');
const test_promise = require(`./build/${common.buildType}/test_promise`);
const assert = require('assert');

let expected_result, promise;

// A resolution
expected_result = 42;
promise = test_promise.createPromise();
promise.then(
  common.mustCall(function(result) {
    assert.strictEqual(result, expected_result,
                       'promise resolved as expected');
  }),
  common.mustNotCall());
test_promise.concludeCurrentPromise(expected_result, true);

// A rejection
expected_result = 'It\'s not you, it\'s me.';
promise = test_promise.createPromise();
promise.then(
  common.mustNotCall(),
  common.mustCall(function(result) {
    assert.strictEqual(result, expected_result,
                       'promise rejected as expected');
  }));
test_promise.concludeCurrentPromise(expected_result, false);

// Chaining
promise = test_promise.createPromise();
promise.then(
  common.mustCall(function(result) {
    assert.strictEqual(result, 'chained answer',
                       'resolving with a promise chains properly');
  }),
  common.mustNotCall());
test_promise.concludeCurrentPromise(Promise.resolve('chained answer'), true);

assert.strictEqual(test_promise.isPromise(promise), true,
                   'natively created promise is recognized as a promise');

assert.strictEqual(test_promise.isPromise(Promise.reject(-1)), true,
                   'Promise created with JS is recognized as a promise');

assert.strictEqual(test_promise.isPromise(2.4), false,
                   'Number is recognized as not a promise');

assert.strictEqual(test_promise.isPromise('I promise!'), false,
                   'String is recognized as not a promise');

assert.strictEqual(test_promise.isPromise(undefined), false,
                   'undefined is recognized as not a promise');

assert.strictEqual(test_promise.isPromise(null), false,
                   'null is recognized as not a promise');

assert.strictEqual(test_promise.isPromise({}), false,
                   'an object is recognized as not a promise');

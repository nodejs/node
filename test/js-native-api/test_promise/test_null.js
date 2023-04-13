'use strict';
const common = require('../../common');
const assert = require('assert');

// Test passing NULL to object-related N-APIs.
const { testNull } = require(`./build/${common.buildType}/test_promise`);

const expectedForCreatePromise = {
  envIsNull: 'Invalid argument',
  deferredIsNull: 'Invalid argument',
  promiseIsNull: 'Invalid argument',
};
assert.deepStrictEqual(testNull.createPromise(), expectedForCreatePromise);

function testPromiseResolution(resultObject) {
  const expectedForResolution = {
    envIsNull: 'Invalid argument',
    deferredIsNull: 'Invalid argument',
    valueIsNull: 'Invalid argument',
  };
  const promise = resultObject.promise;

  delete resultObject.promise;
  assert.deepStrictEqual(resultObject, expectedForResolution);

  // Must avoid an unhandled rejection.
  promise.catch(() => {});
}

testPromiseResolution(testNull.resolveDeferred());
testPromiseResolution(testNull.rejectDeferred());

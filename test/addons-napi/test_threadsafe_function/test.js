'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing api calls for asynchronous function calls
const addon = require(`./build/${common.buildType}/binding`);

new Promise(function testBasic(resolve, reject) {
  const values = [];
  const returnValues = [];
  const expectedReturnValues = [];
  function evaluation() {
    assert.deepStrictEqual(values, [0, 1, 2, 3, 4]);
    assert.deepStrictEqual(returnValues, expectedReturnValues);
  }

  addon.StartThread(
    (value) => {
      values.push(value);
      if (values.length === 5) {
        setImmediate(() => {
          addon.StopThread();
          resolve(evaluation);
        });
        expectedReturnValues.push(new Error('Intentional error'));
        throw expectedReturnValues.slice(-1)[0];
      }
      expectedReturnValues.push(!!(value % 2));
      return expectedReturnValues.slice(-1)[0];
    },
    (returnValue) => (returnValues.push(returnValue)));
}).then(function(evaluation) {
  evaluation();
}).then(function() {
  return new Promise(function testEmpty(resolve, reject) {
    addon.StartEmptyThread(common.mustCall(() => {
      setImmediate(() => {
        addon.StopEmptyThread();
        resolve();
      });
    }));
  });
}).then(function() {
  return new Promise(function testEmptyWithException(resolve, reject) {
    addon.StartEmptyThread(() => {
      const exception = new Error('Empty with exception');
      setImmediate(() => {

        // The exception thrown below will be attributed to StopEmptyThread.
        try {
          addon.StopEmptyThread();
        } catch (anException) {
          assert.strictEqual(anException, exception);
        }
        resolve();
      });
      throw exception;
    });
  });
});

'use strict';
// Flags: --expose-gc

const common = require('../../common');
const assert = require('assert');

// Testing api calls for function
const test_function = require(`./build/${common.buildType}/test_function`);

function func1() {
  return 1;
}
assert.strictEqual(test_function.TestCall(func1), 1);

function func2() {
  console.log('hello world!');
  return null;
}
assert.strictEqual(test_function.TestCall(func2), null);

function func3(input) {
  return input + 1;
}
assert.strictEqual(test_function.TestCall(func3, 1), 2);

function func4(input) {
  return func3(input);
}
assert.strictEqual(test_function.TestCall(func4, 1), 2);

assert.strictEqual(test_function.TestName.name, 'Name');
assert.strictEqual(test_function.TestNameShort.name, 'Name_');

let tracked_function = test_function.MakeTrackedFunction(common.mustCall());
assert(!!tracked_function);
tracked_function = null;
global.gc();

assert.deepStrictEqual(test_function.TestCreateFunctionParameters(), {
  envIsNull: 'Invalid argument',
  nameIsNull: 'napi_ok',
  cbIsNull: 'Invalid argument',
  resultIsNull: 'Invalid argument',
});

assert.throws(
  () => test_function.TestBadReturnExceptionPending(),
  {
    code: 'throwing exception',
    name: 'Error',
  },
);

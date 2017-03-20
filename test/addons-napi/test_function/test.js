'use strict';
const common = require('../../common');
const assert = require('assert');

// testing api calls for function
const test_function = require(`./build/${common.buildType}/test_function`);


function func1() {
  return 1;
}
assert.strictEqual(test_function.Test(func1), 1);

function func2() {
  console.log('hello world!');
  return null;
}
assert.strictEqual(test_function.Test(func2), null);

function func3(input) {
  return input + 1;
}
assert.strictEqual(test_function.Test(func3, 1), 2);

function func4(input) {
  return func3(input);
}
assert.strictEqual(test_function.Test(func4, 1), 2);

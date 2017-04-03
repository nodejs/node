'use strict';
const common = require('../../common');
const assert = require('assert');

// Testing api calls for arrays
const test_array = require(`./build/${common.buildType}/test_array`);

const array = [
  1,
  9,
  48,
  13493,
  9459324,
  { name: 'hello' },
  [
    'world',
    'node',
    'abi'
  ]
];

assert.strictEqual(test_array.Test(array, array.length + 1),
                   'Index out of bound!');

assert.throws(
  () => {
    test_array.Test(array, -2);
  },
  /Invalid index\. Expects a positive integer\./
);

array.forEach(function(element, index) {
  assert.strictEqual(test_array.Test(array, index), element);
});


assert.deepStrictEqual(test_array.New(array), array);

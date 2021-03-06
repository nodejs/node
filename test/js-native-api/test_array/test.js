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

assert.throws(
  () => {
    test_array.TestGetElement(array, array.length + 1);
  },
  /^Error: assertion \(\(\(uint32_t\)index < length\)\) failed: Index out of bounds!$/
);

assert.throws(
  () => {
    test_array.TestGetElement(array, -2);
  },
  /^Error: assertion \(index >= 0\) failed: Invalid index\. Expects a positive integer\.$/
);

array.forEach(function(element, index) {
  assert.strictEqual(test_array.TestGetElement(array, index), element);
});


assert.deepStrictEqual(test_array.New(array), array);

assert(test_array.TestHasElement(array, 0));
assert.strictEqual(test_array.TestHasElement(array, array.length + 1), false);

assert(test_array.NewWithLength(0) instanceof Array);
assert(test_array.NewWithLength(1) instanceof Array);
// Check max allowed length for an array 2^32 -1
assert(test_array.NewWithLength(4294967295) instanceof Array);

{
  // Verify that array elements can be deleted.
  const arr = ['a', 'b', 'c', 'd'];

  assert.strictEqual(arr.length, 4);
  assert.strictEqual(2 in arr, true);
  assert.strictEqual(test_array.TestDeleteElement(arr, 2), true);
  assert.strictEqual(arr.length, 4);
  assert.strictEqual(2 in arr, false);
}

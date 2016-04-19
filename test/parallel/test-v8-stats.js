'use strict';
require('../common');
var assert = require('assert');
var v8 = require('v8');

var s = v8.getHeapStatistics();
var keys = [
  'heap_size_limit',
  'total_available_size',
  'total_heap_size',
  'total_heap_size_executable',
  'total_physical_size',
  'used_heap_size'];
assert.deepStrictEqual(Object.keys(s).sort(), keys);
keys.forEach(function(key) {
  assert.equal(typeof s[key], 'number');
});


const expectedHeapSpaces = [
  'new_space',
  'old_space',
  'code_space',
  'map_space',
  'large_object_space'
];
const heapSpaceStatistics = v8.getHeapSpaceStatistics();
const actualHeapSpaceNames = heapSpaceStatistics.map((s) => s.space_name);
assert.deepStrictEqual(actualHeapSpaceNames.sort(), expectedHeapSpaces.sort());
heapSpaceStatistics.forEach((heapSpace) => {
  assert.strictEqual(typeof heapSpace.space_name, 'string');
  assert.strictEqual(typeof heapSpace.space_size, 'number');
  assert.strictEqual(typeof heapSpace.space_used_size, 'number');
  assert.strictEqual(typeof heapSpace.space_available_size, 'number');
  assert.strictEqual(typeof heapSpace.physical_space_size, 'number');
});

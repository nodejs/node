'use strict';
require('../common');
const assert = require('assert');
const v8 = require('v8');

const s = v8.getHeapStatistics();
const keys = [
  'does_zap_garbage',
  'heap_size_limit',
  'malloced_memory',
  'peak_malloced_memory',
  'total_available_size',
  'total_heap_size',
  'total_heap_size_executable',
  'total_physical_size',
  'used_heap_size'];
assert.deepStrictEqual(Object.keys(s).sort(), keys);
keys.forEach(function(key) {
  assert.strictEqual(typeof s[key], 'number');
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

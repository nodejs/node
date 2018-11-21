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

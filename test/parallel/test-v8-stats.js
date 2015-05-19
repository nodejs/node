'use strict';
var common = require('../common');
var assert = require('assert');
var v8 = require('v8');

var s = v8.getHeapStatistics();
var keys = [
  'heap_size_limit',
  'total_heap_size',
  'total_heap_size_executable',
  'total_physical_size',
  'used_heap_size'];
assert.deepEqual(Object.keys(s).sort(), keys);
keys.forEach(function(key) {
  assert.equal(typeof s[key], 'number');
});

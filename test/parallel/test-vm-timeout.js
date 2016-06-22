'use strict';
require('../common');
var assert = require('assert');
var vm = require('vm');

// Test 1: Timeout of 100ms executing endless loop
assert.throws(function() {
  vm.runInThisContext('while(true) {}', { timeout: 100 });
});

// Test 2: Timeout must be >= 0ms
assert.throws(function() {
  vm.runInThisContext('', { timeout: -1 });
}, RangeError);

// Test 3: Timeout of 0ms
assert.throws(function() {
  vm.runInThisContext('', { timeout: 0 });
}, RangeError);

// Test 4: Timeout of 1000ms, script finishes first
vm.runInThisContext('', { timeout: 1000 });

// Test 5: Nested vm timeouts, inner timeout propagates out
assert.throws(function() {
  var context = {
    log: console.log,
    runInVM: function(timeout) {
      vm.runInNewContext('while(true) {}', context, { timeout: timeout });
    }
  };
  vm.runInNewContext('runInVM(10)', context, { timeout: 10000 });
  throw new Error('Test 5 failed');
}, /Script execution timed out./);

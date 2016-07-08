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

// Test 6: Nested vm timeouts, outer timeout is shorter and fires first.
assert.throws(function() {
  const context = {
    runInVM: function(timeout) {
      vm.runInNewContext('while(true) {}', context, { timeout: timeout });
    }
  };
  vm.runInNewContext('runInVM(10000)', context, { timeout: 100 });
  throw new Error('Test 6 failed');
}, /Script execution timed out./);

// Test 7: Nested vm timeouts, inner script throws an error.
assert.throws(function() {
  const context = {
    runInVM: function(timeout) {
      vm.runInNewContext('throw new Error(\'foobar\')', context, {
        timeout: timeout
      });
    }
  };
  vm.runInNewContext('runInVM(10000)', context, { timeout: 100000 });
}, /foobar/);

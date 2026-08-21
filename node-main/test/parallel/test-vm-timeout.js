// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
require('../common');
const assert = require('assert');
const vm = require('vm');

// Timeout of 100ms executing endless loop
assert.throws(
  function() {
    vm.runInThisContext('while(true) {}', { timeout: 100 });
  },
  {
    code: 'ERR_SCRIPT_EXECUTION_TIMEOUT',
    message: 'Script execution timed out after 100ms'
  });

// Timeout of 1000ms, script finishes first
vm.runInThisContext('', { timeout: 1000 });

// Nested vm timeouts, inner timeout propagates out
assert.throws(
  function() {
    const context = {
      log: console.log,
      runInVM: function(timeout) {
        vm.runInNewContext('while(true) {}', context, { timeout });
      }
    };
    vm.runInNewContext('runInVM(10)', context, { timeout: 10000 });
    throw new Error('Test 5 failed');
  },
  {
    code: 'ERR_SCRIPT_EXECUTION_TIMEOUT',
    message: 'Script execution timed out after 10ms'
  });

// Nested vm timeouts, outer timeout is shorter and fires first.
assert.throws(
  function() {
    const context = {
      runInVM: function(timeout) {
        vm.runInNewContext('while(true) {}', context, { timeout });
      }
    };
    vm.runInNewContext('runInVM(10000)', context, { timeout: 100 });
    throw new Error('Test 6 failed');
  },
  {
    code: 'ERR_SCRIPT_EXECUTION_TIMEOUT',
    message: 'Script execution timed out after 100ms'
  });

// Nested vm timeouts, inner script throws an error.
assert.throws(function() {
  const context = {
    runInVM: function(timeout) {
      vm.runInNewContext('throw new Error(\'foobar\')', context, { timeout });
    }
  };
  vm.runInNewContext('runInVM(10000)', context, { timeout: 100000 });
}, /foobar/);

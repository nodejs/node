'use strict';
var common = require('../common');
var assert = require('assert');

var order = [],
    exceptionHandled = false;

// This nextTick function will throw an error.  It should only be called once.
// When it throws an error, it should still get removed from the queue.
process.nextTick(function() {
  order.push('A');
  // cause an error
  what();
});

// This nextTick function should remain in the queue when the first one
// is removed.  It should be called if the error in the first one is
// caught (which we do in this test).
process.nextTick(function() {
  order.push('C');
});

process.on('uncaughtException', function() {
  if (!exceptionHandled) {
    exceptionHandled = true;
    order.push('B');
  }
  else {
    // If we get here then the first process.nextTick got called twice
    order.push('OOPS!');
  }
});

process.on('exit', function() {
  assert.deepEqual(['A', 'B', 'C'], order);
});


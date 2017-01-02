'use strict';
require('../common');
var assert = require('assert');

const order = [];
let exceptionHandled = false;

// This nextTick function will throw an error.  It should only be called once.
// When it throws an error, it should still get removed from the queue.
process.nextTick(function() {
  order.push('A');
  // cause an error
  what(); // eslint-disable-line no-undef
});

// This nextTick function should remain in the queue when the first one
// is removed.  It should be called if the error in the first one is
// caught (which we do in this test).
process.nextTick(function() {
  order.push('C');
});

function testNextTickWith(val) {
  assert.throws(
    function() {
      process.nextTick(val);
    },
    TypeError
  );
}

testNextTickWith(false);
testNextTickWith(true);
testNextTickWith(1);
testNextTickWith('str');
testNextTickWith({});
testNextTickWith([]);

process.on('uncaughtException', function() {
  if (!exceptionHandled) {
    exceptionHandled = true;
    order.push('B');
  } else {
    // If we get here then the first process.nextTick got called twice
    order.push('OOPS!');
  }
});

process.on('exit', function() {
  assert.deepStrictEqual(['A', 'B', 'C'], order);
});

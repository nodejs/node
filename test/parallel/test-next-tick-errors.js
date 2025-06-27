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
  assert.throws(() => {
    process.nextTick(val);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError'
  });
}

testNextTickWith(false);
testNextTickWith(true);
testNextTickWith(1);
testNextTickWith('str');
testNextTickWith({});
testNextTickWith([]);

process.on('uncaughtException', function(err, errorOrigin) {
  assert.strictEqual(errorOrigin, 'uncaughtException');

  if (!exceptionHandled) {
    exceptionHandled = true;
    order.push('B');
  } else {
    // If we get here then the first process.nextTick got called twice
    order.push('OOPS!');
  }
});

process.on('exit', function() {
  assert.deepStrictEqual(order, ['A', 'B', 'C']);
});

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

var common = require('../common');
var assert = require('assert');

function onAsync() {}

var results = [];
var asyncNoHandleError = {
  error: function(stor) {
    results.push(1);
  }
};

var asyncHandleError = {
  error: function(stor) {
    results.push(0);
    return true;
  }
};

var listeners = [
  process.addAsyncListener(onAsync, asyncHandleError),
  process.addAsyncListener(onAsync, asyncNoHandleError)
];

// Even if an error handler returns true, both should fire.
process.nextTick(function() {
  throw new Error();
});

process.removeAsyncListener(listeners[0]);
process.removeAsyncListener(listeners[1]);

process.on('exit', function(code) {
  // If the exit code isn't ok then return early to throw the stack that
  // caused the bad return code.
  if (code !== 0)
    return;

  // Mixed handling of errors should propagate to all listeners.
  assert.equal(results[0], 0);
  assert.equal(results[1], 1);
  assert.equal(results.length, 2);

  console.log('ok');
});

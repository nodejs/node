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
var tracing = require('tracing');

function onAsync0() {
  return 0;
}

function onAsync1() {
  return 1;
}

function onError(stor) {
  results.push(stor);
}

var results = [];
var asyncNoHandleError0 = {
  create: onAsync0,
  error: onError
};
var asyncNoHandleError1 = {
  create: onAsync1,
  error: onError
};

var listeners = [
  tracing.addAsyncListener(asyncNoHandleError0),
  tracing.addAsyncListener(asyncNoHandleError1)
];

var uncaughtFired = false;
process.on('uncaughtException', function() {
  uncaughtFired = true;

  // Unhandled errors should propagate to all listeners.
  assert.equal(results[0], 0);
  assert.equal(results[1], 1);
  assert.equal(results.length, 2);
});

process.nextTick(function() {
  throw new Error();
});

process.on('exit', function(code) {
  // If the exit code isn't ok then return early to throw the stack that
  // caused the bad return code.
  if (code !== 0)
    return;

  // Need to remove the async listeners or tests will always pass
  for (var i = 0; i < listeners.length; i++)
    tracing.removeAsyncListener(listeners[i]);

  assert.ok(uncaughtFired);
  console.log('ok');
});

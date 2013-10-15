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

var once = 0;
function onAsync0() { }
function onAsync1() { }

var results = [];
var handlers = {
  before: function() {
    throw 1;
  },
  error: function(stor, err) {
    // Must catch error thrown in before callback.
    assert.equal(err, 1);
    once++;
    return true;
  }
}

var handlers1 = {
  before: function() {
    throw 2;
  },
  error: function(stor, err) {
    // Must catch *other* handlers throw by error callback.
    assert.equal(err, 1);
    once++;
    return true;
  }
}

var listeners = [
  process.addAsyncListener(onAsync0, handlers),
  process.addAsyncListener(onAsync1, handlers1)
];

var uncaughtFired = false;
process.on('uncaughtException', function(err) {
  uncaughtFired = true;

  // Both error handlers must fire.
  assert.equal(once, 2);
});

process.nextTick(function() { });

for (var i = 0; i < listeners.length; i++)
  process.removeAsyncListener(listeners[i]);

process.on('exit', function(code) {
  // If the exit code isn't ok then return early to throw the stack that
  // caused the bad return code.
  if (code !== 0)
    return;
  // Make sure uncaughtException actually fired.
  assert.ok(uncaughtFired);
  console.log('ok');
});


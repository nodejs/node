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

var addListener = process.addAsyncListener;
var removeListener = process.removeAsyncListener;
var caught = [];
var expect = [];

function asyncL(a) {}

var callbacksObj = {
  error: function(value, er) {
    process._rawDebug('caught', er.message);
    caught.push(er.message);
    return (expect.indexOf(er.message) !== -1);
  }
};

var listener = process.createAsyncListener(asyncL, callbacksObj);

process.on('exit', function(code) {
  removeListener(listener);

  if (code > 0)
    return;

  expect = expect.sort();
  caught = caught.sort();

  process._rawDebug('expect', expect);
  process._rawDebug('caught', caught);
  assert.deepEqual(caught, expect, 'caught all expected errors');
  process._rawDebug('ok');
});


expect.push('immediate simple a');
expect.push('immediate simple b');
process.nextTick(function() {
  addListener(listener);
  // Tests for a setImmediate specific bug encountered while implementing
  // AsyncListeners.
  setImmediate(function() {
    throw new Error('immediate simple a');
  });
  setImmediate(function() {
    throw new Error('immediate simple b');
  });
  removeListener(listener);
});

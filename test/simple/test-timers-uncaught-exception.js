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

var exceptions = 0;
var timer1 = 0;
var timer2 = 0;

// the first timer throws...
setTimeout(function() {
  timer1++;
  throw new Error('BAM!');
}, 100);

// ...but the second one should still run
setTimeout(function() {
  assert.equal(timer1, 1);
  timer2++;
}, 100);

function uncaughtException(err) {
  assert.equal(err.message, 'BAM!');
  exceptions++;
}
process.on('uncaughtException', uncaughtException);

process.on('exit', function() {
  process.removeListener('uncaughtException', uncaughtException);
  assert.equal(exceptions, 1);
  assert.equal(timer1, 1);
  assert.equal(timer2, 1);
});

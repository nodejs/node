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

// Flags: --expose_gc

var common = require('../common');
var assert = require('assert');
var v8 = require('tracing').v8;

assert(typeof gc === 'function', 'Run this test with --expose_gc.');

var ncalls = 0;
var before;
var after;

function ongc(before_, after_) {
  // Try very hard to not create garbage because that could kick off another
  // garbage collection cycle.
  before = before_;
  after = after_;
  ncalls += 1;
}

gc();
v8.on('gc', ongc);
gc();
v8.removeListener('gc', ongc);
gc();

assert.equal(ncalls, 1);
assert.equal(typeof before, 'object');
assert.equal(typeof after, 'object');
assert.equal(typeof before.timestamp, 'number');
assert.equal(typeof after.timestamp, 'number');
assert.equal(before.timestamp <= after.timestamp, true);

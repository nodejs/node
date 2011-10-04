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

// This test asserts that Stream.prototype.pipe does not leave listeners
// hanging on the source or dest.

var stream = require('stream');
var assert = require('assert');
var util = require('util');

function Writable() {
  this.writable = true;
  this.endCalls = 0;
  stream.Stream.call(this);
}
util.inherits(Writable, stream.Stream);
Writable.prototype.end = function() {
  this.endCalls++;
};

Writable.prototype.destroy = function() {
  this.endCalls++;
};

function Readable() {
  this.readable = true;
  stream.Stream.call(this);
}
util.inherits(Readable, stream.Stream);

var i = 0;
var limit = 100;

var w = new Writable();

console.error = function(text) {
  throw new Error(text);
};

var r;

for (i = 0; i < limit; i++) {
  r = new Readable();
  r.pipe(w);
  r.emit('end');
}
assert.equal(0, r.listeners('end').length);
assert.equal(limit, w.endCalls);

w.endCalls = 0;

for (i = 0; i < limit; i++) {
  r = new Readable();
  r.pipe(w);
  r.emit('close');
}
assert.equal(0, r.listeners('close').length);
assert.equal(limit, w.endCalls);

w.endCalls = 0;

var r2;
r = new Readable();
r2 = new Readable();

r.pipe(w);
r2.pipe(w);
r.emit('close');
r2.emit('close');
assert.equal(1, w.endCalls);

r = new Readable();

for (i = 0; i < limit; i++) {
  w = new Writable();
  r.pipe(w);
  w.emit('end');
}
assert.equal(0, w.listeners('end').length);

for (i = 0; i < limit; i++) {
  w = new Writable();
  r.pipe(w);
  w.emit('close');
}
assert.equal(0, w.listeners('close').length);


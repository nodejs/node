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
// This test asserts that Stream.prototype.pipe does not leave listeners
// hanging on the source or dest.
require('../common');
const stream = require('stream');
const assert = require('assert');

function Writable() {
  this.writable = true;
  this.endCalls = 0;
  stream.Stream.call(this);
}
Object.setPrototypeOf(Writable.prototype, stream.Stream.prototype);
Object.setPrototypeOf(Writable, stream.Stream);
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
Object.setPrototypeOf(Readable.prototype, stream.Stream.prototype);
Object.setPrototypeOf(Readable, stream.Stream);

function Duplex() {
  this.readable = true;
  Writable.call(this);
}
Object.setPrototypeOf(Duplex.prototype, Writable.prototype);
Object.setPrototypeOf(Duplex, Writable);

let i = 0;
const limit = 100;

let w = new Writable();

let r;

for (i = 0; i < limit; i++) {
  r = new Readable();
  r.pipe(w);
  r.emit('end');
}
assert.strictEqual(r.listeners('end').length, 0);
assert.strictEqual(w.endCalls, limit);

w.endCalls = 0;

for (i = 0; i < limit; i++) {
  r = new Readable();
  r.pipe(w);
  r.emit('close');
}
assert.strictEqual(r.listeners('close').length, 0);
assert.strictEqual(w.endCalls, limit);

w.endCalls = 0;

r = new Readable();

for (i = 0; i < limit; i++) {
  w = new Writable();
  r.pipe(w);
  w.emit('close');
}
assert.strictEqual(w.listeners('close').length, 0);

r = new Readable();
w = new Writable();
const d = new Duplex();
r.pipe(d); // pipeline A
d.pipe(w); // pipeline B
assert.strictEqual(r.listeners('end').length, 2);   // A.onend, A.cleanup
assert.strictEqual(r.listeners('close').length, 2); // A.onclose, A.cleanup
assert.strictEqual(d.listeners('end').length, 2);   // B.onend, B.cleanup
// A.cleanup, B.onclose, B.cleanup
assert.strictEqual(d.listeners('close').length, 3);
assert.strictEqual(w.listeners('end').length, 0);
assert.strictEqual(w.listeners('close').length, 1); // B.cleanup

r.emit('end');
assert.strictEqual(d.endCalls, 1);
assert.strictEqual(w.endCalls, 0);
assert.strictEqual(r.listeners('end').length, 0);
assert.strictEqual(r.listeners('close').length, 0);
assert.strictEqual(d.listeners('end').length, 2);   // B.onend, B.cleanup
assert.strictEqual(d.listeners('close').length, 2); // B.onclose, B.cleanup
assert.strictEqual(w.listeners('end').length, 0);
assert.strictEqual(w.listeners('close').length, 1); // B.cleanup

d.emit('end');
assert.strictEqual(d.endCalls, 1);
assert.strictEqual(w.endCalls, 1);
assert.strictEqual(r.listeners('end').length, 0);
assert.strictEqual(r.listeners('close').length, 0);
assert.strictEqual(d.listeners('end').length, 0);
assert.strictEqual(d.listeners('close').length, 0);
assert.strictEqual(w.listeners('end').length, 0);
assert.strictEqual(w.listeners('close').length, 0);

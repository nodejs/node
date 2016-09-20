'use strict';
// This test asserts that Stream.prototype.pipe does not leave listeners
// hanging on the source or dest.
require('../common');
const stream = require('stream');
const assert = require('assert');
const util = require('util');

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

function Duplex() {
  this.readable = true;
  Writable.call(this);
}
util.inherits(Duplex, Writable);

let i = 0;
const limit = 100;

let w = new Writable();

let r;

for (i = 0; i < limit; i++) {
  r = new Readable();
  r.pipe(w);
  r.emit('end');
}
assert.strictEqual(0, r.listeners('end').length);
assert.strictEqual(limit, w.endCalls);

w.endCalls = 0;

for (i = 0; i < limit; i++) {
  r = new Readable();
  r.pipe(w);
  r.emit('close');
}
assert.strictEqual(0, r.listeners('close').length);
assert.strictEqual(limit, w.endCalls);

w.endCalls = 0;

r = new Readable();

for (i = 0; i < limit; i++) {
  w = new Writable();
  r.pipe(w);
  w.emit('close');
}
assert.strictEqual(0, w.listeners('close').length);

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

'use strict';
// This test asserts that Stream.prototype.pipe does not leave listeners
// hanging on the source or dest.

require('../common');
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

function Duplex() {
  this.readable = true;
  Writable.call(this);
}
util.inherits(Duplex, Writable);

var i = 0;
var limit = 100;

var w = new Writable();

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

r = new Readable();

for (i = 0; i < limit; i++) {
  w = new Writable();
  r.pipe(w);
  w.emit('close');
}
assert.equal(0, w.listeners('close').length);

r = new Readable();
w = new Writable();
var d = new Duplex();
r.pipe(d); // pipeline A
d.pipe(w); // pipeline B
assert.equal(r.listeners('end').length, 2);   // A.onend, A.cleanup
assert.equal(r.listeners('close').length, 2); // A.onclose, A.cleanup
assert.equal(d.listeners('end').length, 2);   // B.onend, B.cleanup
assert.equal(d.listeners('close').length, 3); // A.cleanup, B.onclose, B.cleanup
assert.equal(w.listeners('end').length, 0);
assert.equal(w.listeners('close').length, 1); // B.cleanup

r.emit('end');
assert.equal(d.endCalls, 1);
assert.equal(w.endCalls, 0);
assert.equal(r.listeners('end').length, 0);
assert.equal(r.listeners('close').length, 0);
assert.equal(d.listeners('end').length, 2);   // B.onend, B.cleanup
assert.equal(d.listeners('close').length, 2); // B.onclose, B.cleanup
assert.equal(w.listeners('end').length, 0);
assert.equal(w.listeners('close').length, 1); // B.cleanup

d.emit('end');
assert.equal(d.endCalls, 1);
assert.equal(w.endCalls, 1);
assert.equal(r.listeners('end').length, 0);
assert.equal(r.listeners('close').length, 0);
assert.equal(d.listeners('end').length, 0);
assert.equal(d.listeners('close').length, 0);
assert.equal(w.listeners('end').length, 0);
assert.equal(w.listeners('close').length, 0);

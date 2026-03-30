'use strict';

const common = require('../common');
const EventEmitter = require('events');
const assert = require('assert');

const myEE = new EventEmitter();
let m = 0;
// This one comes last.
myEE.on('foo', common.mustCall(() => assert.strictEqual(m, 2)));

// This one comes second.
myEE.prependListener('foo', common.mustCall(() => assert.strictEqual(m++, 1)));

// This one comes first.
myEE.prependOnceListener('foo',
                         common.mustCall(() => assert.strictEqual(m++, 0)));

myEE.emit('foo');

// Test fallback if prependListener is undefined.
const stream = require('stream');

delete EventEmitter.prototype.prependListener;

function Writable() {
  this.writable = true;
  stream.Stream.call(this);
}
Object.setPrototypeOf(Writable.prototype, stream.Stream.prototype);
Object.setPrototypeOf(Writable, stream.Stream);

function Readable() {
  this.readable = true;
  stream.Stream.call(this);
}
Object.setPrototypeOf(Readable.prototype, stream.Stream.prototype);
Object.setPrototypeOf(Readable, stream.Stream);

const w = new Writable();
const r = new Readable();
r.pipe(w);

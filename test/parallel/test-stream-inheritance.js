'use strict';
require('../common');
const assert = require('assert');
const { Readable, Writable, Duplex, Transform } = require('stream');

const readable = new Readable({ read() {} });
const writable = new Writable({ write() {} });
const duplex = new Duplex({ read() {}, write() {} });
const transform = new Transform({ transform() {} });

assert.ok(readable instanceof Readable);
assert.ok(!(writable instanceof Readable));
assert.ok(duplex instanceof Readable);
assert.ok(transform instanceof Readable);

assert.ok(!(readable instanceof Writable));
assert.ok(writable instanceof Writable);
assert.ok(duplex instanceof Writable);
assert.ok(transform instanceof Writable);

assert.ok(!(readable instanceof Duplex));
assert.ok(!(writable instanceof Duplex));
assert.ok(duplex instanceof Duplex);
assert.ok(transform instanceof Duplex);

assert.ok(!(readable instanceof Transform));
assert.ok(!(writable instanceof Transform));
assert.ok(!(duplex instanceof Transform));
assert.ok(transform instanceof Transform);

assert.ok(!(null instanceof Writable));
assert.ok(!(undefined instanceof Writable));

// Simple inheritance check for `Writable` works fine in a subclass constructor.
function CustomWritable() {
  assert.ok(
    this instanceof CustomWritable,
    `${this} does not inherit from CustomWritable`
  );
  assert.ok(
    this instanceof Writable,
    `${this} does not inherit from Writable`
  );
}

Object.setPrototypeOf(CustomWritable, Writable);
Object.setPrototypeOf(CustomWritable.prototype, Writable.prototype);

new CustomWritable();

assert.throws(
  CustomWritable,
  {
    code: 'ERR_ASSERTION',
    constructor: assert.AssertionError,
    message: 'undefined does not inherit from CustomWritable'
  }
);

class OtherCustomWritable extends Writable {}

assert(!(new OtherCustomWritable() instanceof CustomWritable));
assert(!(new CustomWritable() instanceof OtherCustomWritable));

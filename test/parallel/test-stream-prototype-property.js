'use strict';

require('../common');

// This test ensure state has initialized when accessing property
// from prototype.
// Make sure it wouldn't throw TypeError when accessing
// readableLength, readableHighWaterMark from
// stream.Readable.prototype. Same for `Duplex` and `Writable`.

const { Duplex, Readable, Writable } = require('stream');
const assert = require('assert');

const defaultHighWaterMark = 16 * 1024;

{
  function MyDuplex() {
    assert.strictEqual(this.readableHighWaterMark, defaultHighWaterMark);
    assert.strictEqual(this.readableFlowing, false);
    assert.strictEqual(this.readableLength, 0);
    assert.strictEqual(this.writableHighWaterMark, defaultHighWaterMark);
    assert.strictEqual(this.writableLength, 0);
    Duplex.call(this);
  }

  Object.setPrototypeOf(MyDuplex.prototype, Duplex.prototype);
  Object.setPrototypeOf(MyDuplex, Duplex);

  new MyDuplex();
}

{
  function MyReadable() {
    assert.strictEqual(this.readableHighWaterMark, defaultHighWaterMark);
    assert.strictEqual(this.readableFlowing, false);
    assert.strictEqual(this.readableLength, 0);
    Readable.call(this);
  }

  Object.setPrototypeOf(MyReadable.prototype, Readable.prototype);
  Object.setPrototypeOf(MyReadable, Readable);

  new MyReadable();
}

{
  function MyWritable() {
    assert.strictEqual(this.writableHighWaterMark, defaultHighWaterMark);
    assert.strictEqual(this.writableLength, 0);
    Writable.call(this);
  }

  Object.setPrototypeOf(MyWritable.prototype, Writable.prototype);
  Object.setPrototypeOf(MyWritable, Writable);

  new MyWritable();
}

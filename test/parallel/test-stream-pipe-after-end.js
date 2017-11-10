'use strict';
const common = require('../common');
const assert = require('assert');
const Readable = require('_stream_readable');
const Writable = require('_stream_writable');

class TestReadable extends Readable {
  constructor(opt) {
    super(opt);
    this._ended = false;
  }

  _read() {
    if (this._ended)
      this.emit('error', new Error('_read called twice'));
    this._ended = true;
    this.push(null);
  }
}

class TestWritable extends Writable {
  constructor(opt) {
    super(opt);
    this._written = [];
  }

  _write(chunk, encoding, cb) {
    this._written.push(chunk);
    cb();
  }
}

// this one should not emit 'end' until we read() from it later.
const ender = new TestReadable();

// what happens when you pipe() a Readable that's already ended?
const piper = new TestReadable();
// pushes EOF null, and length=0, so this will trigger 'end'
piper.read();

setTimeout(common.mustCall(function() {
  ender.on('end', common.mustCall());
  const c = ender.read();
  assert.strictEqual(c, null);

  const w = new TestWritable();
  w.on('finish', common.mustCall());
  piper.pipe(w);
}), 1);

'use strict';
require('../common');
const assert = require('assert');

const stream = require('stream');

class MyWritable extends stream.Writable {
  constructor(fn, options) {
    super(options);
    this.fn = fn;
  }

  _write(chunk, encoding, callback) {
    this.fn(Buffer.isBuffer(chunk), typeof chunk, encoding);
    callback();
  }
}

{
  const m = new MyWritable(function(isBuffer, type, enc) {
    assert(isBuffer);
    assert.strictEqual(type, 'object');
    assert.strictEqual(enc, 'buffer');
  }, { decodeStrings: true });
  m.write('some-text', 'utf8');
  m.end();
}

{
  const m = new MyWritable(function(isBuffer, type, enc) {
    assert(!isBuffer);
    assert.strictEqual(type, 'string');
    assert.strictEqual(enc, 'utf8');
  }, { decodeStrings: false });
  m.write('some-text', 'utf8');
  m.end();
}

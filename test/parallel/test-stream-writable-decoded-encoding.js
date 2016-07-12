'use strict';
require('../common');
var assert = require('assert');

var stream = require('stream');
var util = require('util');

function MyWritable(fn, options) {
  stream.Writable.call(this, options);
  this.fn = fn;
}

util.inherits(MyWritable, stream.Writable);

MyWritable.prototype._write = function(chunk, encoding, callback) {
  this.fn(Buffer.isBuffer(chunk), typeof chunk, encoding);
  callback();
};

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

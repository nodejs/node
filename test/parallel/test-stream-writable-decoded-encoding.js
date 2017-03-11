'use strict';
require('../common');
const assert = require('assert');

const stream = require('stream');
const util = require('util');

function MyWritable(fn, options) {
  stream.Writable.call(this, options);
  this.fn = fn;
}

util.inherits(MyWritable, stream.Writable);

MyWritable.prototype._write = function(chunk, encoding, callback) {
  this.fn(Buffer.isBuffer(chunk), typeof chunk, encoding);
  callback();
};

(function decodeStringsTrue() {
  const m = new MyWritable(function(isBuffer, type, enc) {
    assert(isBuffer);
    assert.equal(type, 'object');
    assert.equal(enc, 'buffer');
    console.log('ok - decoded string is decoded');
  }, { decodeStrings: true });
  m.write('some-text', 'utf8');
  m.end();
})();

(function decodeStringsFalse() {
  const m = new MyWritable(function(isBuffer, type, enc) {
    assert(!isBuffer);
    assert.equal(type, 'string');
    assert.equal(enc, 'utf8');
    console.log('ok - un-decoded string is not decoded');
  }, { decodeStrings: false });
  m.write('some-text', 'utf8');
  m.end();
})();

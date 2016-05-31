'use strict';
require('../common');
var assert = require('assert');
var stream = require('stream');

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

(function decodeStringsTrue() {
  var m = new MyWritable(function(isBuffer, type, enc) {
    assert(isBuffer);
    assert.equal(type, 'object');
    assert.equal(enc, 'buffer');
    console.log('ok - decoded string is decoded');
  }, { decodeStrings: true });
  m.write('some-text', 'utf8');
  m.end();
})();

(function decodeStringsFalse() {
  var m = new MyWritable(function(isBuffer, type, enc) {
    assert(!isBuffer);
    assert.equal(type, 'string');
    assert.equal(enc, 'utf8');
    console.log('ok - un-decoded string is not decoded');
  }, { decodeStrings: false });
  m.write('some-text', 'utf8');
  m.end();
})();

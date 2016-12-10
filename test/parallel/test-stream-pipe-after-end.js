'use strict';
require('../common');
const assert = require('assert');
const Readable = require('_stream_readable');
const Writable = require('_stream_writable');
const util = require('util');

util.inherits(TestReadable, Readable);
function TestReadable(opt) {
  if (!(this instanceof TestReadable))
    return new TestReadable(opt);
  Readable.call(this, opt);
  this._ended = false;
}

TestReadable.prototype._read = function(n) {
  if (this._ended)
    this.emit('error', new Error('_read called twice'));
  this._ended = true;
  this.push(null);
};

util.inherits(TestWritable, Writable);
function TestWritable(opt) {
  if (!(this instanceof TestWritable))
    return new TestWritable(opt);
  Writable.call(this, opt);
  this._written = [];
}

TestWritable.prototype._write = function(chunk, encoding, cb) {
  this._written.push(chunk);
  cb();
};

// this one should not emit 'end' until we read() from it later.
const ender = new TestReadable();
let enderEnded = false;

// what happens when you pipe() a Readable that's already ended?
const piper = new TestReadable();
// pushes EOF null, and length=0, so this will trigger 'end'
piper.read();

setTimeout(function() {
  ender.on('end', function() {
    enderEnded = true;
  });
  assert(!enderEnded);
  const c = ender.read();
  assert.equal(c, null);

  const w = new TestWritable();
  let writableFinished = false;
  w.on('finish', function() {
    writableFinished = true;
  });
  piper.pipe(w);

  process.on('exit', function() {
    assert(enderEnded);
    assert(writableFinished);
    console.log('ok');
  });
});

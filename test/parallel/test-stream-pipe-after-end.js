'use strict';
require('../common');
var assert = require('assert');

var Readable = require('_stream_readable');
var Writable = require('_stream_writable');

class TestReadable extends Readable {
  constructor(opt) {
    super(opt);
    this._ended = false;
  }

  _read(n) {
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
var ender = new TestReadable();
var enderEnded = false;

// what happens when you pipe() a Readable that's already ended?
var piper = new TestReadable();
// pushes EOF null, and length=0, so this will trigger 'end'
piper.read();

setTimeout(function() {
  ender.on('end', function() {
    enderEnded = true;
  });
  assert(!enderEnded);
  var c = ender.read();
  assert.equal(c, null);

  var w = new TestWritable();
  var writableFinished = false;
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

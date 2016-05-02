'use strict';
require('../common');
var R = require('_stream_readable');
var W = require('_stream_writable');
var assert = require('assert');

var ondataCalled = 0;

class TestReader extends R {
  constructor() {
    super();
    this._buffer = Buffer.alloc(100, 'x');

    this.on('data', function() {
      ondataCalled++;
    });
  }

  _read(n) {
    this.push(this._buffer);
    this._buffer = Buffer.alloc(0);
  }
}

var reader = new TestReader();
setImmediate(function() {
  assert.equal(ondataCalled, 1);
  console.log('ok');
  reader.push(null);
});

class TestWriter extends W {
  constructor() {
    super();
    this.write('foo');
    this.end();
  }

  _write(chunk, enc, cb) {
    cb();
  }
}

var writer = new TestWriter();

process.on('exit', function() {
  assert.strictEqual(reader.readable, false);
  assert.strictEqual(writer.writable, false);
  console.log('ok');
});

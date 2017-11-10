'use strict';
require('../common');
const R = require('_stream_readable');
const W = require('_stream_writable');
const assert = require('assert');

let ondataCalled = 0;

class TestReader extends R {
  constructor() {
    super();
    this._buffer = Buffer.alloc(100, 'x');

    this.on('data', () => {
      ondataCalled++;
    });
  }

  _read(n) {
    this.push(this._buffer);
    this._buffer = Buffer.alloc(0);
  }
}

const reader = new TestReader();
setImmediate(function() {
  assert.strictEqual(ondataCalled, 1);
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

const writer = new TestWriter();

process.on('exit', function() {
  assert.strictEqual(reader.readable, false);
  assert.strictEqual(writer.writable, false);
  console.log('ok');
});

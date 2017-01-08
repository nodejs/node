'use strict';
require('../common');
const R = require('_stream_readable');
const W = require('_stream_writable');
const assert = require('assert');

const util = require('util');

let ondataCalled = 0;

function TestReader() {
  R.apply(this);
  this._buffer = Buffer.alloc(100, 'x');

  this.on('data', function() {
    ondataCalled++;
  });
}

util.inherits(TestReader, R);

TestReader.prototype._read = function(n) {
  this.push(this._buffer);
  this._buffer = Buffer.alloc(0);
};

const reader = new TestReader();
setImmediate(function() {
  assert.strictEqual(ondataCalled, 1);
  console.log('ok');
  reader.push(null);
});

function TestWriter() {
  W.apply(this);
  this.write('foo');
  this.end();
}

util.inherits(TestWriter, W);

TestWriter.prototype._write = function(chunk, enc, cb) {
  cb();
};

const writer = new TestWriter();

process.on('exit', function() {
  assert.strictEqual(reader.readable, false);
  assert.strictEqual(writer.writable, false);
  console.log('ok');
});

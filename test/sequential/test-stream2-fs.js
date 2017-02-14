'use strict';
const common = require('../common');
const assert = require('assert');

const fs = require('fs');
const FSReadable = fs.ReadStream;

const path = require('path');
const file = path.resolve(common.fixturesDir, 'x1024.txt');

const size = fs.statSync(file).size;

const expectLengths = [1024];

const util = require('util');
const Stream = require('stream');

util.inherits(TestWriter, Stream);

function TestWriter() {
  Stream.apply(this);
  this.buffer = [];
  this.length = 0;
}

TestWriter.prototype.write = function(c) {
  this.buffer.push(c.toString());
  this.length += c.length;
  return true;
};

TestWriter.prototype.end = function(c) {
  if (c) this.buffer.push(c.toString());
  this.emit('results', this.buffer);
};

const r = new FSReadable(file);
const w = new TestWriter();

w.on('results', function(res) {
  console.error(res, w.length);
  assert.strictEqual(w.length, size);
  assert.deepStrictEqual(res.map(function(c) {
    return c.length;
  }), expectLengths);
  console.log('ok');
});

r.pipe(w);

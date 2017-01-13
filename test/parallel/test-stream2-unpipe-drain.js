'use strict';
require('../common');
const assert = require('assert');

const stream = require('stream');
const util = require('util');

function TestWriter() {
  stream.Writable.call(this);
}
util.inherits(TestWriter, stream.Writable);

TestWriter.prototype._write = function(buffer, encoding, callback) {
  console.log('write called');
  // super slow write stream (callback never called)
};

const dest = new TestWriter();

function TestReader(id) {
  stream.Readable.call(this);
  this.reads = 0;
}
util.inherits(TestReader, stream.Readable);

TestReader.prototype._read = function(size) {
  this.reads += 1;
  this.push(Buffer.alloc(size));
};

const src1 = new TestReader();
const src2 = new TestReader();

src1.pipe(dest);

src1.once('readable', function() {
  process.nextTick(function() {

    src2.pipe(dest);

    src2.once('readable', function() {
      process.nextTick(function() {

        src1.unpipe(dest);
      });
    });
  });
});


process.on('exit', function() {
  assert.strictEqual(src1.reads, 2);
  assert.strictEqual(src2.reads, 2);
});

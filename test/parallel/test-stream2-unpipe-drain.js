var common = require('../common');
var assert = require('assert');
var stream = require('stream');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var crypto = require('crypto');

var util = require('util');

function TestWriter() {
    stream.Writable.call(this);
}
util.inherits(TestWriter, stream.Writable);

TestWriter.prototype._write = function (buffer, encoding, callback) {
  console.log('write called');
  // super slow write stream (callback never called)
};

var dest = new TestWriter();

function TestReader(id) {
    stream.Readable.call(this);
    this.reads = 0;
}
util.inherits(TestReader, stream.Readable);

TestReader.prototype._read = function (size) {
  this.reads += 1;
  this.push(crypto.randomBytes(size));
};

var src1 = new TestReader();
var src2 = new TestReader();

src1.pipe(dest);

src1.once('readable', function () {
  process.nextTick(function () {

    src2.pipe(dest);

    src2.once('readable', function () {
      process.nextTick(function () {

        src1.unpipe(dest);
      });
    });
  });
});


process.on('exit', function () {
  assert.equal(src1.reads, 2);
  assert.equal(src2.reads, 2);
});

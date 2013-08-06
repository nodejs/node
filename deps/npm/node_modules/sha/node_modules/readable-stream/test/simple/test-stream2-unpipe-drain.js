// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.


var common = require('../common.js');
var assert = require('assert');
var stream = require('../../readable');
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

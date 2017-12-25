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

'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const stream = fs.createWriteStream(`${tmpdir.path}/out`, {
  highWaterMark: 10
});
const err = new Error('BAM');

const write = fs.write;
let writeCalls = 0;
fs.write = function() {
  switch (writeCalls++) {
    case 0:
      console.error('first write');
      // first time is ok.
      return write.apply(fs, arguments);
    case 1:
      // then it breaks
      console.error('second write');
      const cb = arguments[arguments.length - 1];
      return process.nextTick(function() {
        cb(err);
      });
    default:
      // and should not be called again!
      throw new Error('BOOM!');
  }
};

fs.close = common.mustCall(function(fd_, cb) {
  console.error('fs.close', fd_, stream.fd);
  assert.strictEqual(fd_, stream.fd);
  process.nextTick(cb);
});

stream.on('error', common.mustCall(function(err_) {
  console.error('error handler');
  assert.strictEqual(stream.fd, null);
  assert.strictEqual(err_, err);
}));


stream.write(Buffer.allocUnsafe(256), function() {
  console.error('first cb');
  stream.write(Buffer.allocUnsafe(256), common.mustCall(function(err_) {
    console.error('second cb');
    assert.strictEqual(err_, err);
  }));
});

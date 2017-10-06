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

const stream = fs.createReadStream(__filename, {
  bufferSize: 64
});
const err = new Error('BAM');

stream.on('error', common.mustCall((err_) => {
  process.nextTick(common.mustCall(() => {
    assert.strictEqual(stream.fd, null);
    assert.strictEqual(err_, err);
  }));
}));

fs.close = common.mustCall((fd_, cb) => {
  assert.strictEqual(fd_, stream.fd);
  process.nextTick(cb);
});

const read = fs.read;
fs.read = function() {
  // first time is ok.
  read.apply(fs, arguments);
  // then it breaks
  fs.read = common.mustCall(function() {
    const cb = arguments[arguments.length - 1];
    process.nextTick(() => {
      cb(err);
    });
    // and should not be called again!
    fs.read = () => {
      throw new Error('BOOM!');
    };
  });
};

stream.on('data', (buf) => {
  stream.on('data', common.mustNotCall("no more 'data' events should follow"));
});

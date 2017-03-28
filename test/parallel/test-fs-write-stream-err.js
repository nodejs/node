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

common.refreshTmpDir();

const stream = fs.createWriteStream(common.tmpDir + '/out', {
  highWaterMark: 10
});
const err = new Error('BAM');

const writeBuffer = process.binding('fs').writeBuffer;
let writeCalls = 0;
process.binding('fs').writeBuffer = common.mustCall(function() {
  switch (writeCalls++) {
    case 0:
      // first time is ok.
      return writeBuffer.apply(null, arguments);
    case 1:
      // then it breaks
      const req = arguments[arguments.length - 1];
      return process.nextTick(function() {
        req.oncomplete(err);
      });
  }
}, 2);

process.binding('fs').close = common.mustCall(function(fd, req) {
  assert.strictEqual(fd, stream.fd);
  process.nextTick(req.oncomplete.bind(req));
});

stream.on('error', common.mustCall(function(err_) {
  assert.strictEqual(stream.fd, null);
  assert.strictEqual(err_, err);
}));


stream.write(Buffer.allocUnsafe(256), function() {
  stream.write(Buffer.allocUnsafe(256), common.mustCall(function(err_) {
    assert.strictEqual(err_, err);
  }));
});

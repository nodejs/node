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

var common = require('../common');
var assert = require('assert');
var fs = require('fs');

var stream = fs.createReadStream(__filename, {
  bufferSize: 64
});
var err = new Error('BAM');

stream.on('error', common.mustCall(function errorHandler(err_) {
  console.error('error event');
  process.nextTick(function() {
    assert.equal(stream.fd, null);
    assert.equal(err_, err);
  });
}));

fs.close = common.mustCall(function(fd_, cb) {
  assert.equal(fd_, stream.fd);
  process.nextTick(cb);
});

var read = fs.read;
fs.read = function() {
  // first time is ok.
  read.apply(fs, arguments);
  // then it breaks
  fs.read = function() {
    var cb = arguments[arguments.length - 1];
    process.nextTick(function() {
      cb(err);
    });
    // and should not be called again!
    fs.read = function() {
      throw new Error('BOOM!');
    };
  };
};

stream.on('data', function(buf) {
  stream.on('data', assert.fail);  // no more 'data' events should follow
});

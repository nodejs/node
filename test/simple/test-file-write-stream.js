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

var path = require('path');
var fs = require('fs');
var fn = path.join(common.tmpDir, 'write.txt');
var file = fs.createWriteStream(fn, {
      highWaterMark: 10
    });

var EXPECTED = '012345678910';

var callbacks = {
      open: -1,
      drain: -2,
      close: -1
    };

file
  .on('open', function(fd) {
      console.error('open!');
      callbacks.open++;
      assert.equal('number', typeof fd);
    })
  .on('error', function(err) {
      throw err;
      console.error('error!', err.stack);
    })
  .on('drain', function() {
      console.error('drain!', callbacks.drain);
      callbacks.drain++;
      if (callbacks.drain == -1) {
        assert.equal(EXPECTED, fs.readFileSync(fn, 'utf8'));
        file.write(EXPECTED);
      } else if (callbacks.drain == 0) {
        assert.equal(EXPECTED + EXPECTED, fs.readFileSync(fn, 'utf8'));
        file.end();
      }
    })
  .on('close', function() {
      console.error('close!');
      assert.strictEqual(file.bytesWritten, EXPECTED.length * 2);

      callbacks.close++;
      assert.throws(function() {
        console.error('write after end should not be allowed');
        file.write('should not work anymore');
      });

      fs.unlinkSync(fn);
    });

for (var i = 0; i < 11; i++) {
  (function(i) {
    file.write('' + i);
  })(i);
}

process.on('exit', function() {
  for (var k in callbacks) {
    assert.equal(0, callbacks[k], k + ' count off by ' + callbacks[k]);
  }
  console.log('ok');
});

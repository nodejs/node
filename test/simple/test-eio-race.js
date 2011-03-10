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

var count = 100;
var fs = require('fs');

function tryToKillEventLoop() {
  console.log('trying to kill event loop ...');

  fs.stat(__filename, function(err) {
    if (err) {
      throw new Exception('first fs.stat failed');
    } else {
      console.log('first fs.stat succeeded ...');
      fs.stat(__filename, function(err) {
        if (err) {
          throw new Exception('second fs.stat failed');
        } else {
          console.log('second fs.stat succeeded ...');
          console.log('could not kill event loop, retrying...');

          setTimeout(function() {
            if (--count) {
              tryToKillEventLoop();
            } else {
              process.exit(0);
            }
          }, 1);
        }
      });
    }
  });
}

// Generate a lot of thread pool events
var pos = 0;
fs.open('/dev/zero', 'r', 0666, function(err, fd) {
  if (err) throw err;

  function readChunk() {
    fs.read(fd, 1024, pos, 'binary', function(err, chunk, bytesRead) {
      if (err) throw err;
      if (chunk) {
        pos += bytesRead;
        //console.log(pos);
        readChunk();
      } else {
        fs.closeSync(fd);
        throw new Exception('/dev/zero shouldn\'t end ' +
                            'before the issue shows up');
      }
    });
  }
  readChunk();
});

tryToKillEventLoop();

process.addListener('exit', function() {
  assert.ok(pos > 10000);
});

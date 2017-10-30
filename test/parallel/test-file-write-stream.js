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

const path = require('path');
const fs = require('fs');
const fn = path.join(common.tmpDir, 'write.txt');
common.refreshTmpDir();
const file = fs.createWriteStream(fn, {
  highWaterMark: 10
});

const EXPECTED = '012345678910';

const callbacks = {
  open: -1,
  drain: -2,
  close: -1
};

file
  .on('open', function(fd) {
    console.error('open!');
    callbacks.open++;
    assert.strictEqual('number', typeof fd);
  })
  .on('error', function(err) {
    throw err;
  })
  .on('drain', function() {
    console.error('drain!', callbacks.drain);
    callbacks.drain++;
    if (callbacks.drain === -1) {
      assert.strictEqual(EXPECTED, fs.readFileSync(fn, 'utf8'));
      file.write(EXPECTED);
    } else if (callbacks.drain === 0) {
      assert.strictEqual(EXPECTED + EXPECTED, fs.readFileSync(fn, 'utf8'));
      file.end();
    }
  })
  .on('close', function() {
    console.error('close!');
    assert.strictEqual(file.bytesWritten, EXPECTED.length * 2);

    callbacks.close++;
    common.expectsError(
      () => {
        console.error('write after end should not be allowed');
        file.write('should not work anymore');
      },
      {
        code: 'ERR_STREAM_WRITE_AFTER_END',
        type: Error,
        message: 'write after end'
      }
    );

    fs.unlinkSync(fn);
  });

for (let i = 0; i < 11; i++) {
  file.write(`${i}`);
}

process.on('exit', function() {
  for (const k in callbacks) {
    assert.strictEqual(0, callbacks[k], `${k} count off by ${callbacks[k]}`);
  }
  console.log('ok');
});

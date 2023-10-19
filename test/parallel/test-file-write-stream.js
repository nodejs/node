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
const fn = tmpdir.resolve('write.txt');
tmpdir.refresh();
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
    assert.strictEqual(typeof fd, 'number');
  })
  .on('drain', function() {
    console.error('drain!', callbacks.drain);
    callbacks.drain++;
    if (callbacks.drain === -1) {
      assert.strictEqual(fs.readFileSync(fn, 'utf8'), EXPECTED);
      file.write(EXPECTED);
    } else if (callbacks.drain === 0) {
      assert.strictEqual(fs.readFileSync(fn, 'utf8'), EXPECTED + EXPECTED);
      file.end();
    }
  })
  .on('close', function() {
    console.error('close!');
    assert.strictEqual(file.bytesWritten, EXPECTED.length * 2);

    callbacks.close++;
    file.write('should not work anymore', common.expectsError({
      code: 'ERR_STREAM_WRITE_AFTER_END',
      name: 'Error',
      message: 'write after end'
    }));
    file.on('error', common.mustNotCall());

    fs.unlinkSync(fn);
  });

for (let i = 0; i < 11; i++) {
  file.write(`${i}`);
}

process.on('exit', function() {
  for (const k in callbacks) {
    assert.strictEqual(callbacks[k], 0, `${k} count off by ${callbacks[k]}`);
  }
  console.log('ok');
});

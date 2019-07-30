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
// Flags: --expose_gc

require('../common');
const assert = require('assert');

const tmpdir = require('../common/tmpdir');

function newBuffer(size, value) {
  const buffer = Buffer.allocUnsafe(size);
  while (size--) {
    buffer[size] = value;
  }
  buffer[buffer.length - 1] = 0x0a;
  return buffer;
}

const fs = require('fs');

tmpdir.refresh();
const testFileName = require('path').join(tmpdir.path, 'GH-814_testFile.txt');
const testFileFD = fs.openSync(testFileName, 'w');
console.log(testFileName);


const kBufSize = 128 * 1024;
let PASS = true;
const neverWrittenBuffer = newBuffer(kBufSize, 0x2e); // 0x2e === '.'
const bufPool = [];


const tail = require('child_process').spawn('tail', ['-f', testFileName]);
tail.stdout.on('data', tailCB);

function tailCB(data) {
  PASS = !data.toString().includes('.');
}


const timeToQuit = Date.now() + 8e3; // Test during no more than this seconds.
(function main() {

  if (PASS) {
    fs.write(testFileFD, newBuffer(kBufSize, 0x61), 0, kBufSize, -1, cb);
    global.gc();
    const nuBuf = Buffer.allocUnsafe(kBufSize);
    neverWrittenBuffer.copy(nuBuf);
    if (bufPool.push(nuBuf) > 100) {
      bufPool.length = 0;
    }
  } else {
    throw new Error("Buffer GC'ed test -> FAIL");
  }

  if (Date.now() < timeToQuit) {
    process.nextTick(main);
  } else {
    tail.kill();
    console.log("Buffer GC'ed test -> PASS (OK)");
  }

})();


function cb(err, written) {
  assert.ifError(err);
}

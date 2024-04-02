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
require('../common');
const fs = require('fs');
const assert = require('assert');
const join = require('path').join;

const tmpdir = require('../common/tmpdir');

const filename = join(tmpdir.path, 'out.txt');

tmpdir.refresh();

const fd = fs.openSync(filename, 'w');

const line = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaa\n';

const N = 10240;
let complete = 0;

for (let i = 0; i < N; i++) {
  // Create a new buffer for each write. Before the write is actually
  // executed by the thread pool, the buffer will be collected.
  const buffer = Buffer.from(line);
  fs.write(fd, buffer, 0, buffer.length, null, function(er, written) {
    complete++;
    if (complete === N) {
      fs.closeSync(fd);
      const s = fs.createReadStream(filename);
      s.on('data', testBuffer);
    }
  });
}

let bytesChecked = 0;

function testBuffer(b) {
  for (let i = 0; i < b.length; i++) {
    bytesChecked++;
    if (b[i] !== 'a'.charCodeAt(0) && b[i] !== '\n'.charCodeAt(0)) {
      throw new Error(`invalid char ${i},${b[i]}`);
    }
  }
}

process.on('exit', function() {
  // Probably some of the writes are going to overlap, so we can't assume
  // that we get (N * line.length). Let's just make sure we've checked a
  // few...
  assert.ok(bytesChecked > 1000);
});

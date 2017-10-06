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
const Readable = require('_stream_readable');

let len = 0;
const chunks = new Array(10);
for (let i = 1; i <= 10; i++) {
  chunks[i - 1] = Buffer.allocUnsafe(i);
  len += i;
}

const test = new Readable();
let n = 0;
test._read = function(size) {
  const chunk = chunks[n++];
  setTimeout(function() {
    test.push(chunk === undefined ? null : chunk);
  }, 1);
};

test.on('end', thrower);
function thrower() {
  throw new Error('this should not happen!');
}

let bytesread = 0;
test.on('readable', function() {
  const b = len - bytesread - 1;
  const res = test.read(b);
  if (res) {
    bytesread += res.length;
    console.error('br=%d len=%d', bytesread, len);
    setTimeout(next, 1);
  }
  test.read(0);
});
test.read(0);

function next() {
  // now let's make 'end' happen
  test.removeListener('end', thrower);
  test.on('end', common.mustCall());

  // one to get the last byte
  let r = test.read();
  assert(r);
  assert.strictEqual(r.length, 1);
  r = test.read();
  assert.strictEqual(r, null);
}

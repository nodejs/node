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
const Readable = require('stream').Readable;
const r = new Readable();
const N = 256 * 1024;

// Go ahead and allow the pathological case for this test.
// Yes, it's an infinite loop, that's the point.
process.maxTickDepth = N + 2;

let reads = 0;
r._read = function(n) {
  const chunk = reads++ === N ? null : Buffer.allocUnsafe(1);
  r.push(chunk);
};

r.on('readable', function onReadable() {
  if (!(r._readableState.length % 256))
    console.error('readable', r._readableState.length);
  r.read(N * 2);
});

r.on('end', common.mustCall());

r.read(0);

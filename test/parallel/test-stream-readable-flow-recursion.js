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
const assert = require('assert');

// This test verifies that passing a huge number to read(size)
// will push up the highWaterMark, and cause the stream to read
// more data continuously, but without triggering a nextTick
// warning or RangeError.

const Readable = require('stream').Readable;

// Throw an error if we trigger a nextTick warning.
process.throwDeprecation = true;

const stream = new Readable({ highWaterMark: 2 });
let reads = 0;
let total = 5000;
stream._read = function(size) {
  reads++;
  size = Math.min(size, total);
  total -= size;
  if (size === 0)
    stream.push(null);
  else
    stream.push(Buffer.allocUnsafe(size));
};

let depth = 0;

function flow(stream, size, callback) {
  depth += 1;
  const chunk = stream.read(size);

  if (!chunk)
    stream.once('readable', flow.bind(null, stream, size, callback));
  else
    callback(chunk);

  depth -= 1;
  console.log(`flow(${depth}): exit`);
}

flow(stream, 5000, function() {
  console.log(`complete (${depth})`);
});

process.on('exit', function(code) {
  assert.strictEqual(reads, 2);
  // We pushed up the high water mark
  assert.strictEqual(stream.readableHighWaterMark, 8192);
  // Length is 0 right now, because we pulled it all out.
  assert.strictEqual(stream.readableLength, 0);
  assert(!code);
  assert.strictEqual(depth, 0);
  console.log('ok');
});

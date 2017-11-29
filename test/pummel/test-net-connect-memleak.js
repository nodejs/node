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
// Flags: --expose-gc

const common = require('../common');
const assert = require('assert');
const net = require('net');

console.log('Run this test with --expose-gc');
assert.strictEqual(
  typeof global.gc,
  'function'
);
net.createServer(function() {}).listen(common.PORT);

let before = 0;
{
  // 2**26 == 64M entries
  global.gc();
  let junk = [0];
  for (let i = 0; i < 26; ++i) junk = junk.concat(junk);
  before = process.memoryUsage().rss;

  net.createConnection(common.PORT, '127.0.0.1', function() {
    assert.notStrictEqual(junk.length, 0);  // keep reference alive
    setTimeout(done, 10);
    global.gc();
  });
}

function done() {
  global.gc();
  const after = process.memoryUsage().rss;
  const reclaimed = (before - after) / 1024;
  console.log('%d kB reclaimed', reclaimed);
  assert(reclaimed > 128 * 1024);  // It's around 256 MB on x64.
  process.exit();
}

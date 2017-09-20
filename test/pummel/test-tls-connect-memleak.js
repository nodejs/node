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
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

assert.strictEqual(
  typeof global.gc,
  'function',
  'Run this test with --expose-gc'
);

tls.createServer({
  cert: fixtures.readSync('test_cert.pem'),
  key: fixtures.readSync('test_key.pem')
}).listen(common.PORT);

{
  // 2**26 == 64M entries
  let junk = [0];

  for (let i = 0; i < 26; ++i) junk = junk.concat(junk);

  const options = { rejectUnauthorized: false };
  tls.connect(common.PORT, '127.0.0.1', options, function() {
    assert.notStrictEqual(junk.length, 0);  // keep reference alive
    setTimeout(done, 10);
    global.gc();
  });
}

function done() {
  const before = process.memoryUsage().rss;
  global.gc();
  const after = process.memoryUsage().rss;
  const reclaimed = (before - after) / 1024;
  console.log('%d kB reclaimed', reclaimed);
  assert(reclaimed > 256 * 1024);  // it's more like 512M on x64
  process.exit();
}

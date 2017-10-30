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
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

const buf = Buffer.allocUnsafe(10000);
let received = 0;
const maxChunk = 768;

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
}, function(c) {
  // Lower and upper limits
  assert(!c.setMaxSendFragment(511));
  assert(!c.setMaxSendFragment(16385));

  // Correct fragment size
  assert(c.setMaxSendFragment(maxChunk));

  c.end(buf);
}).listen(0, common.mustCall(function() {
  const c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, common.mustCall(function() {
    c.on('data', function(chunk) {
      assert(chunk.length <= maxChunk);
      received += chunk.length;
    });

    // Ensure that we receive 'end' event anyway
    c.on('end', common.mustCall(function() {
      c.destroy();
      server.close();
      assert.strictEqual(received, buf.length);
    }));
  }));
}));

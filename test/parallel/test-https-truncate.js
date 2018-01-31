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

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const https = require('https');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

// number of bytes discovered empirically to trigger the bug
const data = Buffer.alloc(1024 * 32 + 1);

httpsTest();

function httpsTest() {
  const sopt = { key, cert };

  const server = https.createServer(sopt, function(req, res) {
    res.setHeader('content-length', data.length);
    res.end(data);
    server.close();
  });

  server.listen(0, function() {
    const opts = { port: this.address().port, rejectUnauthorized: false };
    https.get(opts).on('response', function(res) {
      test(res);
    });
  });
}


const test = common.mustCall(function(res) {
  res.on('end', common.mustCall(function() {
    assert.strictEqual(res.readableLength, 0);
    assert.strictEqual(bytes, data.length);
  }));

  // Pause and then resume on each chunk, to ensure that there will be
  // a lone byte hanging out at the very end.
  let bytes = 0;
  res.on('data', function(chunk) {
    bytes += chunk.length;
    this.pause();
    setTimeout(() => { this.resume(); }, 1);
  });
});

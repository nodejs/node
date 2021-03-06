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

// This test tries to confirm that a TLS Socket will work as expected even if it
// is created after the original socket has received some data.
//
// Ref: https://github.com/nodejs/node-v0.x-archive/issues/6940
// Ref: https://github.com/nodejs/node-v0.x-archive/pull/6950

const fixtures = require('../common/fixtures');
const assert = require('assert');
const tls = require('tls');
const net = require('net');

const sent = 'hello world';
let received = '';

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = net.createServer(common.mustCall((c) => {
  setTimeout(function() {
    const s = new tls.TLSSocket(c, {
      isServer: true,
      secureContext: tls.createSecureContext(options)
    });

    s.on('data', (chunk) => {
      received += chunk;
    });

    s.on('end', common.mustCall(() => {
      server.close();
      s.destroy();
    }));
  }, 200);
})).listen(0, common.mustCall(() => {
  const c = tls.connect(server.address().port, {
    rejectUnauthorized: false
  }, () => {
    c.end(sent);
  });
}));

process.on('exit', () => {
  assert.strictEqual(received, sent);
});

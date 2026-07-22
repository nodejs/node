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
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('rsa_private.pem'),
  cert: fixtures.readKey('rsa_cert.crt')
};

const server = tls.createServer(options, function(cleartext) {
  cleartext.end('World');
});

server.once('secureConnection', common.mustCall(function(socket) {
  const cert = socket.getCertificate();
  // The server's local cert is the client's peer cert.
  assert.deepStrictEqual(
    cert.subject.OU,
    ['Test TLS Certificate', 'Engineering']
  );
}));

server.listen(0, common.mustCall(function() {
  const socket = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    const peerCert = socket.getPeerCertificate();
    assert.deepStrictEqual(
      peerCert.subject.OU,
      ['Test TLS Certificate', 'Engineering']
    );
    server.close();
  }));
  socket.end('Hello');
}));

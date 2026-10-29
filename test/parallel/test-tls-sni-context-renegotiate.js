'use strict';

// Once a connection's secure context is chosen, later ClientHellos for the
// same name - the second ClientHello of a TLS 1.3 HelloRetryRequest, or a TLS
// 1.2 renegotiation - must leave it as it is. A context with no certificate of
// its own (here one that only sets `ca`) keeps the server's certificate, and
// must keep it for those too.

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const tls = require('node:tls');
const fixtures = require('../common/fixtures');
const { isBoringSSL } = require('../common/crypto');

const server = tls.createServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
  // Offering only this group makes the server answer a TLS 1.3 client whose
  // first key share is for another one with a HelloRetryRequest.
  ecdhCurve: 'P-521',
}, common.mustCall((socket) => {
  assert.strictEqual(socket.servername, 'certless.example');
  socket.resume();
}, isBoringSSL ? 1 : 2));
server.addContext('certless.example', { ca: fixtures.readKey('ca1-cert.pem') });

function connect(maxVersion) {
  return new Promise((resolve) => {
    const socket = tls.connect({
      port: server.address().port,
      servername: 'certless.example',
      rejectUnauthorized: false,
      maxVersion,
      ecdhCurve: 'P-256:P-521',
    }, () => resolve(socket));
  });
}

server.listen(0, common.mustCall(async () => {
  const retried = await connect('TLSv1.3');
  retried.end();

  // BoringSSL servers don't support renegotiation.
  if (!isBoringSSL) {
    const renegotiating = await connect('TLSv1.2');
    await new Promise((resolve) => {
      renegotiating.renegotiate({}, common.mustSucceed(resolve));
    });
    renegotiating.end();
  }

  server.close();
}));

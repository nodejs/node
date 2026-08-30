'use strict';

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const assert = require('node:assert');
const { X509Certificate } = require('node:crypto');
const fixtures = require('../common/fixtures');
const tls = require('node:tls');

// The certificate has BIT STRING CN entries before and after its valid CN,
// and a SEQUENCE OU entry. OpenSSL accepts the certificate but cannot convert
// those name values to UTF-8.
const cert = fixtures.readSync('x509-name-non-string.pem');
const key = fixtures.readKey('agent1-key.pem');

function checkName(name) {
  assert.strictEqual(name.CN, 'good.example');
  assert.strictEqual(typeof name.CN, 'string');
  assert.strictEqual(name.O, 'GoodOrg');
  assert.strictEqual(Object.hasOwn(name, 'OU'), false);
  assert.strictEqual(Object.hasOwn(name, ''), false);
}

const legacy = new X509Certificate(cert).toLegacyObject();
checkName(legacy.subject);
checkName(legacy.issuer);

const server = tls.createServer({
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem'),
  requestCert: true,
  rejectUnauthorized: false,
}, common.mustCall((socket) => {
  // Exercise the legacy TLS conversion, including detailed chain traversal.
  const detailed = socket.getPeerCertificate(true);
  checkName(detailed.subject);
  checkName(detailed.issuer);

  // The X509Certificate path defers name conversion until legacy inspection.
  const peer = socket.getPeerX509Certificate();
  assert(peer instanceof X509Certificate);
  const peerLegacy = peer.toLegacyObject();
  checkName(peerLegacy.subject);
  checkName(peerLegacy.issuer);
  socket.end();
}));

server.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: server.address().port,
    cert,
    key,
    rejectUnauthorized: false,
  }, common.mustCall(() => client.end()));

  client.on('close', common.mustCall(() => server.close()));
}));

// Exercise the implicit getPeerCertificate(true) call made by tls.connect()
// before checkServerIdentity(). A peer-controlled name value must not escape
// from the connection callback as an uncaught exception.
const malformedServer = tls.createServer({ key, cert }, (socket) => {
  socket.end();
});

malformedServer.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: malformedServer.address().port,
    ca: cert,
    servername: 'good.example',
  }, common.mustCall(() => {
    const detailed = client.getPeerCertificate(true);
    checkName(detailed.subject);
    checkName(detailed.issuer);
    client.end();
  }));

  client.on('close', common.mustCall(() => malformedServer.close()));
}));

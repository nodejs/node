'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Reading the peer certificate must not consume the chain held by the SSL
// session. getPeerX509Certificate() and getPeerCertificate() have to keep
// returning the full chain however often, and in whichever order, they are
// called on either end of the connection (see the #65579 regression, where an
// internal getPeerX509Certificate() call left getPeerCertificate(true) with
// only the leaf). On the server this also covers the peer certificate check
// that runs before 'secureConnection' is emitted.

const assert = require('assert');
const { X509Certificate } = require('crypto');
const tls = require('tls');
const fixtures = require('../common/fixtures');

// Each peer presents a distinct leaf -> intermediate -> root chain, so the
// certificate read back has two issuers above the leaf.
const serverChain = [
  'leaf-from-intermediate-cert.pem',
  'intermediate-ca.pem',
  'fake-startcom-root-cert.pem',
].map((name) => fixtures.readKey(name));
const clientChain = [
  'agent10-cert.pem',
  'ca4-cert.pem',
  'ca2-cert.pem',
].map((name) => fixtures.readKey(name));

function fingerprints(chain) {
  return chain.map((pem) => new X509Certificate(pem).fingerprint256);
}

function checkPeerCertificate(socket, chain, side) {
  assert.strictEqual(socket.authorized, true, side);
  const [leaf, intermediate, root] = fingerprints(chain);

  // Two rounds, alternating the read methods, so a chain consumed by one read
  // would be observed by the next.
  for (let round = 0; round < 2; round++) {
    const x509 = socket.getPeerX509Certificate();
    assert.strictEqual(x509.fingerprint256, leaf, side);
    assert.strictEqual(x509.issuerCertificate.fingerprint256,
                       intermediate, side);

    const detailed = socket.getPeerCertificate(true);
    assert.strictEqual(detailed.fingerprint256, leaf, side);
    assert.strictEqual(detailed.issuerCertificate.fingerprint256,
                       intermediate, side);
    assert.strictEqual(
      detailed.issuerCertificate.issuerCertificate.fingerprint256, root, side);

    assert.strictEqual(socket.getPeerCertificate().fingerprint256, leaf, side);
  }
}

const server = tls.createServer({
  key: fixtures.readKey('leaf-from-intermediate-key.pem'),
  cert: Buffer.concat(serverChain),
  ca: clientChain[2],
  requestCert: true,
}, common.mustCall((socket) => {
  checkPeerCertificate(socket, clientChain, 'server');
  socket.end();
}));

server.listen(0, common.mustCall(() => {
  const socket = tls.connect({
    port: server.address().port,
    key: fixtures.readKey('agent10-key.pem'),
    cert: Buffer.concat(clientChain),
    ca: serverChain[2],
  }, common.mustCall(() => {
    checkPeerCertificate(socket, serverChain, 'client');

    // The client receives the server chain verbatim, so its X509 certificate
    // links all the way to the root, exercising the recursive issuer build
    // more than one level deep.
    const [, , root] = fingerprints(serverChain);
    const x509 = socket.getPeerX509Certificate();
    assert.strictEqual(x509.issuerCertificate.issuerCertificate.fingerprint256,
                       root);
  }));
  socket.on('close', common.mustCall(() => server.close()));
}));

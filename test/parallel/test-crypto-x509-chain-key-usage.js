// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { X509Certificate } = require('node:crypto');
const { deserialize, serialize } = require('node:v8');
const {
  createX509CertificateFromSnapshot,
} = require('internal/crypto/x509');
const fixtures = require('../common/fixtures');

const leaf = new X509Certificate(`-----BEGIN CERTIFICATE-----
MIIBHTCBxaADAgECAgECMAoGCCqGSM49BAMCMBoxGDAWBgNVBAMMD3NuYXBzaG90
IGlzc3VlcjAeFw0yNTAxMDEwMDAwMDBaFw0zNTAxMDEwMDAwMDBaMBgxFjAUBgNV
BAMMDXNuYXBzaG90IGxlYWYwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAAQYBC5j
NN5rAvdjqeCKGhuSxH7DSi35DDi7dArQbeGw5FzdtQmOpglNYbi+TfmcRwkPouuR
i1cCQjEtL+3TW51PMAoGCCqGSM49BAMCA0cAMEQCIE/FmO4pJThTdhYs3kHamBe9
Qo9qR4urR0ZVNoS320q2AiBxs3QJcbhz18plOnwj2n6TTcevxtJauggAca+AVUg+
HQ==
-----END CERTIFICATE-----`);

const issuer = new X509Certificate(`-----BEGIN CERTIFICATE-----
MIIBQjCB6aADAgECAgEBMAoGCCqGSM49BAMCMBoxGDAWBgNVBAMMD3NuYXBzaG90
IGlzc3VlcjAeFw0yNTAxMDEwMDAwMDBaFw0zNTAxMDEwMDAwMDBaMBoxGDAWBgNV
BAMMD3NuYXBzaG90IGlzc3VlcjBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABG4H
eatluz8bc/iTzQN0Ada94gDZpq5E1g+xP/6WAKXILrnvRBZhRBi2xIJExYu1QZq2
+mvnupnjSy7kJUdBz2WjIDAeMAwGA1UdEwQFMAMBAf8wDgYDVR0PAQH/BAQDAgeA
MAoGCCqGSM49BAMCA0gAMEUCIQDqKmAVHvhW0i87KWzp8sjUWId5IVWSgi7iqJx+
los0qwIgOALGVcPyyV56QNJGOIYCcXNLPQ9Adbs2arUVYcWGNEU=
-----END CERTIFICATE-----`);

// The issuer's Key Usage omits keyCertSign. Inspecting or copying the chain
// does not verify its certificates.
assert.strictEqual(leaf.verify(issuer.publicKey), true);
assert.strictEqual(leaf.checkIssued(issuer), false);

function assertChain(certificate, expected) {
  const seen = new Set();
  for (const raw of expected) {
    assert(certificate instanceof X509Certificate);
    assert(!seen.has(certificate));
    seen.add(certificate);
    assert.deepStrictEqual(certificate.raw, raw);
    certificate = certificate.issuerCertificate;
  }
  assert.strictEqual(certificate, undefined);
}

const unrelated = new X509Certificate(fixtures.readKey('agent1-cert.pem'));

// A recognized extension can be malformed without making the enclosing
// certificate invalid DER. Snapshotting does not interpret extensions.
const malformedIssuer = Buffer.from(issuer.raw);
const basicConstraints = Buffer.from('30030101ff', 'hex');
const basicConstraintsOffset = malformedIssuer.indexOf(basicConstraints);
assert.notStrictEqual(basicConstraintsOffset, -1);
malformedIssuer[basicConstraintsOffset] = 0x31;
assert.deepStrictEqual(new X509Certificate(malformedIssuer).raw, malformedIssuer);

for (const expected of [
  [leaf.raw, issuer.raw],
  [leaf.raw, leaf.raw, issuer.raw],
  [leaf.raw, issuer.raw, leaf.raw],
  [issuer.raw, leaf.raw],
  [leaf.raw, unrelated.raw, issuer.raw],
  [leaf.raw, malformedIssuer],
]) {
  const certificate = createX509CertificateFromSnapshot(expected);
  assertChain(certificate, expected);
  assertChain(structuredClone(certificate), expected);
  assertChain(deserialize(serialize(certificate)), expected);
}

for (const snapshot of [
  [],
  [Buffer.alloc(0)],
  [leaf.raw, Buffer.from('not a certificate')],
  [leaf.raw, Buffer.concat([issuer.raw, Buffer.from([0])])],
]) {
  assert.throws(() => createX509CertificateFromSnapshot(snapshot), {
    code: 'ERR_CRYPTO_OPERATION_FAILED',
  });
}

'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const {
  X509Certificate,
  createPrivateKey,
} = require('node:crypto');
const { readFileSync } = require('node:fs');
const { inspect } = require('node:util');
const fixtures = require('../common/fixtures');

const certData = readFileSync(fixtures.path('keys', 'agent1-cert.pem'));
const keyData = readFileSync(fixtures.path('keys', 'agent1-key.pem'));
const caData = readFileSync(fixtures.path('keys', 'ca1-cert.pem'));

const cert = new X509Certificate(certData);
const ca = new X509Certificate(caData);
const privateKey = createPrivateKey(keyData);
const expectedPem = cert.toString();
const expectedSubject = cert.subject;

function assertNoOwnKeys(value) {
  assert.deepStrictEqual(Object.getOwnPropertyNames(value), []);
  assert.deepStrictEqual(Object.getOwnPropertySymbols(value), []);
  assert.deepStrictEqual(Reflect.ownKeys(value), []);
}

for (const name of [
  'subject',
  'subjectAltName',
  'issuer',
  'issuerCertificate',
  'infoAccess',
  'validFrom',
  'validTo',
  'validFromDate',
  'validToDate',
  'fingerprint',
  'fingerprint256',
  'fingerprint512',
  'keyUsage',
  'serialNumber',
  'signatureAlgorithm',
  'signatureAlgorithmOid',
  'raw',
  'publicKey',
  'ca',
]) {
  // Populate every private lazy-cache entry before checking reflection.
  Reflect.get(cert, name);
}
cert.toString();
cert.toJSON();
assertNoOwnKeys(cert);

const getterNames = [
  'subject',
  'subjectAltName',
  'issuer',
  'infoAccess',
  'validFrom',
  'validTo',
  'validFromDate',
  'validToDate',
  'fingerprint',
  'fingerprint256',
  'fingerprint512',
  'keyUsage',
  'serialNumber',
  'signatureAlgorithm',
  'signatureAlgorithmOid',
];
const originals = new Map();
for (const name of getterNames) {
  originals.set(name,
                Object.getOwnPropertyDescriptor(
                  X509Certificate.prototype, name));
  Object.defineProperty(X509Certificate.prototype, name, {
    configurable: true,
    get() { return `FORGED-${name}`; },
  });
}
const originalToString =
  Object.getOwnPropertyDescriptor(X509Certificate.prototype, 'toString');
Object.defineProperty(X509Certificate.prototype, 'toString', {
  configurable: true,
  value() { return 'FORGED-PEM'; },
});

try {
  assert.strictEqual(cert.subject, 'FORGED-subject');
  assert.strictEqual(cert.toString(), 'FORGED-PEM');

  const rendered = inspect(cert, { depth: 4 });
  assert.match(rendered, /CN=agent1/);
  assert.doesNotMatch(rendered, /FORGED/);
  assert.strictEqual(cert.toJSON(), expectedPem);
  assert.strictEqual(JSON.parse(JSON.stringify(cert)), expectedPem);

  assert.strictEqual(cert.checkIssued(ca), true);
  assert.strictEqual(cert.checkPrivateKey(privateKey), true);
  assert.strictEqual(cert.verify(ca.publicKey), true);
  assertNoOwnKeys(cert);
} finally {
  for (const [name, descriptor] of originals) {
    Object.defineProperty(X509Certificate.prototype, name, descriptor);
  }
  Object.defineProperty(
    X509Certificate.prototype, 'toString', originalToString);
}

assert.strictEqual(cert.subject, expectedSubject);
assert.strictEqual(cert.toString(), expectedPem);

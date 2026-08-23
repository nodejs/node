// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('node:assert');
const { X509Certificate } = require('node:crypto');
const { readFileSync } = require('node:fs');
const fixtures = require('../common/fixtures');
const {
  isX509Certificate,
} = require('internal/crypto/x509');

const certData = readFileSync(fixtures.path('keys', 'agent1-cert.pem'));
const cert = new X509Certificate(certData);
const invalidThis = { code: 'ERR_INVALID_THIS', name: 'TypeError' };

const getterNames = [
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
];

const methodNames = [
  'toString',
  'toJSON',
  'checkHost',
  'checkEmail',
  'checkIP',
  'checkIssued',
  'checkPrivateKey',
  'verify',
  'toLegacyObject',
];

assert.strictEqual(isX509Certificate(cert), true);
assert.strictEqual(cert instanceof X509Certificate, true);
assert.strictEqual(Object.getPrototypeOf(X509Certificate.prototype),
                   Object.prototype);
for (const name of ['pem', 'checkCA', 'getIssuerCert', 'toLegacy']) {
  assert.strictEqual(name in cert, false);
}

for (const value of [
  {},
  { __proto__: null },
  1,
  null,
  undefined,
  Buffer.alloc(1),
  function() {},
]) {
  assert.strictEqual(isX509Certificate(value), false);
  for (const name of getterNames) {
    const getter = Object.getOwnPropertyDescriptor(
      X509Certificate.prototype, name).get;
    assert.throws(() => getter.call(value), invalidThis);
  }
  for (const name of methodNames) {
    assert.throws(
      () => X509Certificate.prototype[name].call(value),
      invalidThis);
  }
}

const spoofed = { __proto__: X509Certificate.prototype };
assert.strictEqual(spoofed instanceof X509Certificate, true);
assert.strictEqual(isX509Certificate(spoofed), false);
assert.throws(() => spoofed.subject, invalidThis);
assert.throws(() => spoofed.toString(), invalidThis);

const originalHasInstance =
  Object.getOwnPropertyDescriptor(X509Certificate, Symbol.hasInstance);
Object.defineProperty(X509Certificate, Symbol.hasInstance, {
  configurable: true,
  value: () => true,
});
try {
  const buffer = Buffer.alloc(1);
  assert.strictEqual(buffer instanceof X509Certificate, true);
  assert.strictEqual(isX509Certificate(buffer), false);
  assert.throws(() => X509Certificate.prototype.toString.call(buffer),
                invalidThis);
} finally {
  if (originalHasInstance === undefined) {
    delete X509Certificate[Symbol.hasInstance];
  } else {
    Object.defineProperty(
      X509Certificate, Symbol.hasInstance, originalHasInstance);
  }
}

class DerivedX509Certificate extends X509Certificate {}
const derived = new DerivedX509Certificate(certData);
assert.strictEqual(derived instanceof DerivedX509Certificate, true);
assert.strictEqual(derived instanceof X509Certificate, true);
assert.strictEqual(isX509Certificate(derived), true);
assert.strictEqual(derived.subject, cert.subject);

function PrimitivePrototype() {}
PrimitivePrototype.prototype = 1;
const primitivePrototype = Reflect.construct(
  X509Certificate, [certData], PrimitivePrototype);
assert.strictEqual(Object.getPrototypeOf(primitivePrototype), Object.prototype);
assert.strictEqual(isX509Certificate(primitivePrototype), true);
assert.strictEqual(
  Object.getOwnPropertyDescriptor(
    X509Certificate.prototype, 'subject').get.call(primitivePrototype),
  cert.subject);

const firstPrototype = { __proto__: X509Certificate.prototype };
const secondPrototype = { __proto__: X509Certificate.prototype };
let prototypeReads = 0;
const ProxyNewTarget = new Proxy(function() {}, {
  get(target, property, receiver) {
    if (property === 'prototype') {
      prototypeReads++;
      return prototypeReads === 1 ? firstPrototype : secondPrototype;
    }
    return Reflect.get(target, property, receiver);
  },
});
const proxyNewTarget = Reflect.construct(
  X509Certificate, [certData], ProxyNewTarget);
assert.strictEqual(prototypeReads, 1);
assert.strictEqual(Object.getPrototypeOf(proxyNewTarget), firstPrototype);
assert.strictEqual(proxyNewTarget.subject, cert.subject);

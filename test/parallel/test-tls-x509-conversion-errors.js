'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { isBoringSSL } = require('../common/crypto');
const {
  generateKeyPairSync,
  generateX509CertificateSync,
  X509Certificate,
} = require('crypto');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { connect } = require(fixtures.path('tls-connect'));

const rootKeys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
const issuerKeys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
const serverKeys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
const clientKeys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
const malformedAia = {
  type: 'raw',
  oid: '1.3.6.1.5.5.7.1.1',
  value: Buffer.from('0500', 'hex'),
};
const caExtensions = [
  { type: 'basicConstraints', ca: true, critical: true },
  { type: 'keyUsage', usages: ['keyCertSign', 'crlSign'], critical: true },
];
const san = {
  type: 'subjectAltName',
  names: [{ type: 'dns', value: 'localhost' }],
};
let serialNumber = 1n;

function generate(keys, name, options = {}) {
  return generateX509CertificateSync({
    subject: [{ name: 'CN', value: name }],
    publicKey: keys.publicKey,
    serialNumber: serialNumber++,
    notBefore: new Date('2000-01-01T00:00:00Z'),
    notAfter: new Date('2125-01-01T00:00:00Z'),
    signature: { key: keys.privateKey, digest: 'sha256' },
    ...options,
  });
}

const root = generate(rootKeys, 'Root CA', { extensions: caExtensions });
const signature = { key: rootKeys.privateKey, digest: 'sha256' };
const leaf = generate(serverKeys, 'localhost', {
  issuer: root,
  signature,
  extensions: [san, malformedAia],
});
const issuer = generate(issuerKeys, 'Intermediate CA', {
  issuer: root,
  signature,
  extensions: [...caExtensions, malformedAia],
});
const chainedLeaf = generate(serverKeys, 'localhost', {
  issuer,
  signature: { key: issuerKeys.privateKey, digest: 'sha256' },
  extensions: [san],
});
const clientCertificate = generate(clientKeys, 'client', {
  issuer: root,
  signature,
  extensions: [malformedAia],
});
const serverKey = serverKeys.privateKey.export({ type: 'pkcs8', format: 'pem' });
const clientKey = clientKeys.privateKey.export({ type: 'pkcs8', format: 'pem' });
const clientOptions = {
  host: '127.0.0.1',
  servername: 'localhost',
  ca: root.toString(),
};
const expectedError = { code: 'ERR_CRYPTO_OPERATION_FAILED' };

for (const certificate of [leaf, issuer, clientCertificate]) {
  assert.throws(() => certificate.infoAccess, expectedError);
  assert.throws(() => certificate.toLegacyObject(), expectedError);
}

function assertLegacy(legacy, certificate, name) {
  assert.strictEqual(legacy.subject.CN, name);
  assert.strictEqual(legacy.infoAccess, undefined);
  assert.strictEqual(legacy.fingerprint256, certificate.fingerprint256);
  assert.deepStrictEqual(legacy.raw, certificate.raw);
}

// Default hostname verification must not fail on unrelated malformed metadata.
connect({
  server: { key: serverKey, cert: leaf.toString() },
  client: clientOptions,
}, common.mustSucceed((pair, cleanup) => {
  assert.strictEqual(pair.client.conn.authorized, true);
  assertLegacy(pair.client.conn.getPeerCertificate(), leaf, 'localhost');
  assertLegacy(pair.server.conn.getCertificate(), leaf, 'localhost');
  const x509 = pair.client.conn.getPeerX509Certificate();
  assert.throws(() => x509.infoAccess, expectedError);
  assert.throws(() => x509.toLegacyObject(), expectedError);
  cleanup();
}));

// The full legacy chain passed to a custom identity check remains usable.
connect({
  server: { key: serverKey, cert: `${chainedLeaf}${issuer}` },
  client: {
    ...clientOptions,
    checkServerIdentity: common.mustCall((hostname, certificate) => {
      assert.strictEqual(hostname, 'localhost');
      assert.strictEqual(certificate.subject.CN, 'localhost');
      assertLegacy(certificate.issuerCertificate, issuer, 'Intermediate CA');
      return tls.checkServerIdentity(hostname, certificate);
    }),
  },
}, common.mustSucceed((pair, cleanup) => {
  assert.strictEqual(pair.client.conn.authorized, true);
  const chain = pair.client.conn.getPeerCertificate(true);
  assertLegacy(chain.issuerCertificate, issuer, 'Intermediate CA');
  assert.throws(
    () => pair.client.conn.getPeerX509Certificate().issuerCertificate.infoAccess,
    expectedError,
  );
  cleanup();
}));

// Reading an authenticated client's certificate must use the TLS legacy mode.
connect({
  server: {
    key: serverKey,
    cert: leaf.toString(),
    ca: root.toString(),
    requestCert: true,
    rejectUnauthorized: true,
  },
  client: {
    ...clientOptions,
    key: clientKey,
    cert: clientCertificate.toString(),
  },
}, common.mustSucceed((pair, cleanup) => {
  assert.strictEqual(pair.client.conn.authorized, true);
  assert.strictEqual(pair.server.conn.authorized, true);
  assertLegacy(pair.server.conn.getPeerCertificate(true),
               clientCertificate, 'client');
  assertLegacy(pair.client.conn.getCertificate(), clientCertificate, 'client');
  assert.throws(
    () => pair.server.conn.getPeerX509Certificate().infoAccess,
    expectedError,
  );
  cleanup();
}));

{
  const malformed = generate(serverKeys, 'localhost', {
    issuer: root,
    signature,
    extensions: [san, {
      type: 'raw',
      oid: '2.5.29.37',
      value: Buffer.from('0500', 'hex'),
    }],
  });
  assert.throws(() => malformed.keyUsage, expectedError);
  assert.throws(() => malformed.toLegacyObject(), expectedError);
  connect({
    server: { key: serverKey, cert: malformed.toString() },
    client: { ...clientOptions, rejectUnauthorized: false },
  }, common.mustSucceed((pair, cleanup) => {
    assert.strictEqual(
      pair.client.conn.getPeerCertificate().ext_key_usage, undefined);
    assert.strictEqual(pair.server.conn.getCertificate().ext_key_usage, undefined);
    assert.throws(
      () => pair.client.conn.getPeerX509Certificate().keyUsage, expectedError);
    cleanup();
  }));
}

if (!isBoringSSL) {
  // BoringSSL rejects malformed validity times when parsing the certificate.
  const der = Buffer.from(generate(serverKeys, 'localhost', {
    issuer: root,
    signature,
    extensions: [san],
  }).raw);
  const offset = der.indexOf('000101000000Z');
  assert.notStrictEqual(offset, -1);
  der.write('99', offset + 6, 'ascii');
  const malformed = new X509Certificate(der);
  assert.throws(() => malformed.validFrom, expectedError);
  assert.throws(() => malformed.validFromDate, expectedError);
  assert.throws(() => malformed.toLegacyObject(), expectedError);
  connect({
    server: { key: serverKey, cert: malformed.toString() },
    client: { ...clientOptions, rejectUnauthorized: false },
  }, common.mustSucceed((pair, cleanup) => {
    assert.strictEqual(
      pair.client.conn.getPeerCertificate().valid_from, 'Bad time value');
    assert.strictEqual(
      pair.server.conn.getCertificate().valid_from, 'Bad time value');
    cleanup();
  }));
}

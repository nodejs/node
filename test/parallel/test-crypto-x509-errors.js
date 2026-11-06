'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { isBoringSSL } = require('../common/crypto');
const fixtures = require('../common/fixtures');
const {
  constants,
  generateKeyPairSync,
  generateX509CertificateSync,
  X509Certificate,
} = require('crypto');

const keys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
const expectedError = { code: 'ERR_CRYPTO_OPERATION_FAILED' };
const eku = Buffer.from('300a06082b06010505070301', 'hex');

function generate(overrides = {}) {
  return generateX509CertificateSync({
    subject: [{ name: 'CN', value: 'example.test' }],
    publicKey: keys.publicKey,
    serialNumber: 1n,
    notBefore: new Date('2025-01-02T03:04:05Z'),
    notAfter: new Date('2030-01-02T03:04:05Z'),
    signature: { key: keys.privateKey, digest: 'sha256' },
    ...overrides,
  });
}

const certificate = generate();
assert.strictEqual(certificate.infoAccess, undefined);
assert.strictEqual(certificate.keyUsage, undefined);
assert.strictEqual(certificate.toLegacyObject().infoAccess, undefined);
assert.strictEqual(certificate.toLegacyObject().ext_key_usage, undefined);
assert.strictEqual(certificate.raw, certificate.raw);
assert.strictEqual(certificate.validFromDate, certificate.validFromDate);
assert.strictEqual(certificate.validToDate, certificate.validToDate);
assert.strictEqual(certificate.checkPrivateKey(keys.privateKey), true);
const otherKeys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
assert.strictEqual(certificate.checkPrivateKey(otherKeys.privateKey), false);
assert.strictEqual(certificate.verify(keys.publicKey), true);
assert.strictEqual(certificate.verify(otherKeys.publicKey), false);
const rsaKeys = generateKeyPairSync('rsa', { modulusLength: 2048 });
assert.strictEqual(certificate.verify(rsaKeys.publicKey), false);

{
  const emptySubject = generate({
    subject: [],
    issuer: certificate,
    extensions: [{
      type: 'subjectAltName',
      critical: true,
      names: [{ type: 'dns', value: 'example.test' }],
    }],
  });
  assert.strictEqual(emptySubject.subject, '');
  assert.deepStrictEqual(emptySubject.toLegacyObject().subject, {
    __proto__: null,
  });
}

{
  const aia = generate({
    extensions: [{
      type: 'raw',
      oid: '1.3.6.1.5.5.7.1.1',
      value: Buffer.from(
        '301e301c06082b060105050730018610687474703a2f2f6f6373702e74657374',
        'hex'),
    }],
  });
  assert.match(aia.infoAccess, /^OCSP - URI:http:\/\/ocsp\.test\n?$/);
}

for (const [oid, property] of [
  ['1.3.6.1.5.5.7.1.1', 'infoAccess'],
  ['2.5.29.37', 'keyUsage'],
]) {
  const malformed = generate({
    extensions: [{ type: 'raw', oid, value: Buffer.from('0500', 'hex') }],
  });
  assert.throws(() => malformed[property], expectedError);
  assert.throws(() => malformed.toLegacyObject(), expectedError);
}

for (const value of [eku, Buffer.from('3000', 'hex')]) {
  const withEku = generate({
    extensions: [{ type: 'raw', oid: '2.5.29.37', value }],
  });
  assert.deepStrictEqual(
    withEku.keyUsage, value === eku ? ['1.3.6.1.5.5.7.3.1'] : []);
  assert.strictEqual(withEku.keyUsage, withEku.keyUsage);
}

{
  const duplicateEku = generate({
    extensions: [
      { type: 'raw', oid: '2.5.29.37', value: eku },
      { type: 'raw', oid: '2.5.29.99', value: eku },
    ],
  });
  const der = Buffer.from(duplicateEku.raw);
  const oid = Buffer.from('0603551d63', 'hex');
  const offset = der.indexOf(oid);
  assert.notStrictEqual(offset, -1);
  // Change the second extension's OID without changing its encoded size.
  der[offset + oid.length - 1] = 37;
  const malformed = new X509Certificate(der);
  assert.throws(() => malformed.keyUsage, expectedError);
  assert.throws(() => malformed.toLegacyObject(), expectedError);
}

if (!isBoringSSL) {
  // BoringSSL rejects malformed validity times while parsing the certificate.
  const fixture = new X509Certificate(fixtures.readKey('agent1-cert.pem'));
  for (const [encoded, property, dateProperty] of [
    ['220903214037Z', 'validFrom', 'validFromDate'],
    ['22960617214037Z', 'validTo', 'validToDate'],
  ]) {
    const der = Buffer.from(fixture.raw);
    const offset = der.indexOf(encoded);
    assert.notStrictEqual(offset, -1);
    // Replace the hour while retaining the UTCTime or GeneralizedTime size.
    der.write('99', offset + encoded.length - 7, 'ascii');
    const malformed = new X509Certificate(der);
    assert.throws(() => malformed[property], expectedError);
    assert.throws(() => malformed[dateProperty], expectedError);
    assert.throws(() => malformed.toLegacyObject(), expectedError);
  }

  const der = Buffer.from(certificate.raw);
  const algorithm = Buffer.from('06072a8648ce3d0201', 'hex');
  const offset = der.indexOf(algorithm);
  assert.notStrictEqual(offset, -1);
  // Replace id-ecPublicKey with an unknown algorithm of the same encoded size.
  der[offset + algorithm.length - 1] = 2;
  const malformed = new X509Certificate(der);
  assert.throws(() => malformed.checkPrivateKey(keys.privateKey), expectedError);
}

function assertVerifyError(certificate, publicKey, reason) {
  // BoringSSL reports verification errors as zero rather than a negative value.
  if (isBoringSSL) {
    assert.strictEqual(certificate.verify(publicKey), false);
  } else {
    assert.throws(() => certificate.verify(publicKey), { reason });
  }
}

{
  const der = Buffer.from(certificate.raw);
  const algorithm = Buffer.from('06082a8648ce3d040302', 'hex');
  const offsets = [der.indexOf(algorithm), der.lastIndexOf(algorithm)];
  assert.notStrictEqual(offsets[0], -1);
  assert.notStrictEqual(offsets[0], offsets[1]);
  // Replace ecdsa-with-SHA256 in both AlgorithmIdentifiers with an unknown OID.
  for (const offset of offsets)
    der[offset + algorithm.length - 1] = 0x7f;
  assertVerifyError(new X509Certificate(der), keys.publicKey,
                    'unknown signature algorithm');
}

{
  const der = Buffer.from(certificate.raw);
  const signature = certificate.signature.value;
  const offset = der.lastIndexOf(signature);
  assert(offset > 0);
  assert.strictEqual(der[offset - 1], 0);
  // Encode one unused bit in the signature BIT STRING, with its padding zero.
  der[offset - 1] = 1;
  der[offset + signature.length - 1] &= 0xfe;
  assert.strictEqual(new X509Certificate(der).verify(keys.publicKey), false);
}

{
  const pss = generate({
    publicKey: rsaKeys.publicKey,
    signature: {
      key: {
        key: rsaKeys.privateKey,
        padding: constants.RSA_PKCS1_PSS_PADDING,
        saltLength: 32,
      },
      digest: 'sha256',
    },
  });
  assert.strictEqual(pss.verify(rsaKeys.publicKey), true);
  assert.strictEqual(pss.verify(keys.publicKey), false);
  const der = Buffer.from(pss.raw);
  const saltLength = Buffer.from('a203020120', 'hex');
  const offsets = [der.indexOf(saltLength), der.lastIndexOf(saltLength)];
  assert.notStrictEqual(offsets[0], -1);
  assert.notStrictEqual(offsets[0], offsets[1]);
  // Make the RSA-PSS salt length negative in both AlgorithmIdentifiers.
  for (const offset of offsets)
    der[offset + saltLength.length - 1] = 0xff;
  // OpenSSL reports this as a generic internal error, which stays an exception.
  assertVerifyError(new X509Certificate(der), rsaKeys.publicKey,
                    'internal error');
}

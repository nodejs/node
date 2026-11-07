'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { hasFIPS, isBoringSSL } = require('../common/crypto');
const {
  constants,
  createPublicKey,
  generateKeyPairSync,
  generateX509Certificate,
  generateX509CertificateSync,
  X509Certificate,
} = require('crypto');
const { promisify } = require('util');

const generationModule = 'NativeModule internal/crypto/x509_generation';
assert(!process.moduleLoadList.includes(generationModule));

const notBefore = new Date('2025-01-02T03:04:05.999Z');
const notAfter = new Date('2050-01-02T03:04:05.123Z');
const subjectKeyIdentifierOid = '2.5.29.14';
const authorityKeyIdentifierOid = '2.5.29.35';
const encodedSubject = Buffer.from(
  '3011310f300d060355040313066167656e7431', 'hex');
const generateX509CertificateAsync = promisify(generateX509Certificate);

function findExtension(certificate, oid) {
  const extension = certificate.extensions.find((entry) => entry.oid === oid);
  assert(extension);
  return extension;
}

function getSubjectKeyIdentifier(certificate) {
  const { value } = findExtension(certificate, subjectKeyIdentifierOid);
  assert.deepStrictEqual(value.subarray(0, 2), Buffer.from([0x04, 0x14]));
  assert.strictEqual(value.length, 22);
  return value.subarray(2);
}

function getAuthorityKeyIdentifier(certificate) {
  const { value } = findExtension(certificate, authorityKeyIdentifierOid);
  assert.deepStrictEqual(
    value.subarray(0, 4), Buffer.from([0x30, 0x16, 0x80, 0x14]));
  assert.strictEqual(value.length, 24);
  return value.subarray(4);
}

function baseOptions(keys, overrides = {}) {
  return {
    subject: [{ name: 'CN', value: 'example.com' }],
    publicKey: keys.publicKey,
    serialNumber: 1n,
    notBefore,
    notAfter,
    signature: {
      key: keys.privateKey,
      digest: keys.publicKey.asymmetricKeyType === 'ed25519' ?
        null : 'sha256',
    },
    ...overrides,
  };
}

function assertSigned(certificate, publicKey, privateKey) {
  assert(certificate instanceof X509Certificate);
  assert.strictEqual(certificate.verify(publicKey), true);
  if (privateKey !== undefined)
    assert.strictEqual(certificate.checkPrivateKey(privateKey), true);
}

function assertSynchronousFailure(options, expected) {
  assert.throws(() => generateX509CertificateSync(options), expected);
  assert.throws(() => generateX509Certificate(
    options, common.mustNotCall()), expected);
}

// This certificate has a valid empty subject with a critical SAN, but it cannot
// supply the nonempty issuer name required when generating another certificate.
const emptySubjectIssuer = new X509Certificate(`-----BEGIN CERTIFICATE-----
MIIBDTCBtaADAgECAgEBMAoGCCqGSM49BAMCMAAwHhcNMjUwMTAxMDAwMDAwWhcN
MzAwMTAxMDAwMDAwWjAAMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE3AsDY6Mw
cXKGY4R96+CAApyMKVVxUj/vkSU05Fo//J1cC32pINc7r3DSCEgbcbSgl5dau7wm
62/uYVKbS3Uwm6MgMB4wHAYDVR0RAQH/BBIwEIIOaXNzdWVyLmV4YW1wbGUwCgYI
KoZIzj0EAwIDRwAwRAIhAM4l65D0ooMAWBw8LdoN/mGSEM7lQhCawDxHRN+Dqzl6
Ah97+DipkjdevW5E+ACvluxAP30ZJCjMak2QtuQjyC7b
-----END CERTIFICATE-----`);
assert.strictEqual(emptySubjectIssuer.subject, '');

const caKeys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
const ca = generateX509CertificateSync(baseOptions(caKeys, {
  subject: [
    [
      { name: 'O', value: 'Node.js' },
      { name: 'OU', value: 'Crypto' },
    ],
    { name: 'C', value: 'CZ' },
    { name: '1.2.3.4', value: 'custom' },
  ],
  serialNumber: (1n << 159n) - 1n,
  extensions: [
    { type: 'basicConstraints', ca: true, pathLength: 1 },
    { type: 'keyUsage', usages: ['keyCertSign', 'crlSign'], critical: true },
    { type: 'authorityKeyIdentifier', value: 'auto' },
    { type: 'subjectKeyIdentifier', value: 'auto' },
  ],
}));

assert(process.moduleLoadList.includes(generationModule));
assertSigned(ca, caKeys.publicKey, caKeys.privateKey);
assert.strictEqual(ca.ca, true);
assert.strictEqual(ca.issuerCertificate, undefined);
assert.strictEqual(ca.validFromDate.toISOString(), '2025-01-02T03:04:05.000Z');
assert.strictEqual(ca.validToDate.toISOString(), '2050-01-02T03:04:05.000Z');
assert.strictEqual(
  ca.subject,
  'OU=Crypto + O=Node.js\nC=CZ\n1.2.3.4=custom',
);
assert.strictEqual(ca.issuer, ca.subject);
assert.deepStrictEqual(
  getAuthorityKeyIdentifier(ca), getSubjectKeyIdentifier(ca));

{
  const expectedSubject = ca.subjectName.raw;
  const backing = Buffer.concat([
    Buffer.from([0xff]),
    expectedSubject,
    Buffer.from([0xff]),
  ]);
  const subject = new DataView(
    backing.buffer, backing.byteOffset + 1, expectedSubject.length);
  const certificate = generateX509CertificateSync(baseOptions(caKeys, {
    subject,
  }));
  assert.deepStrictEqual(certificate.subjectName.raw, expectedSubject);
  assert.deepStrictEqual(certificate.issuerName.raw, expectedSubject);
}

// RFC 7093 method 1 hashes only the subjectPublicKey BIT STRING contents and
// truncates the SHA-256 digest to its leftmost 160 bits.
{
  const publicKey = createPublicKey({
    key: Buffer.from(
      '3059301306072a8648ce3d020106082a8648ce3d030107034200' +
      '047f7f35a79794c950060b8029fc8f363a28f11159692d9d34' +
      'e6ac948190434735f833b1a66652dc514337aff7f5c9c75d67' +
      '0c019d95a5d639b72744c64a9128bb',
      'hex'),
    format: 'der',
    type: 'spki',
  });
  const certificate = generateX509CertificateSync(baseOptions(
    { publicKey, privateKey: caKeys.privateKey },
    {
      issuer: ca,
      signature: { key: caKeys.privateKey, digest: 'sha256' },
      extensions: [{ type: 'subjectKeyIdentifier', value: 'auto' }],
    },
  ));
  assert.deepStrictEqual(
    getSubjectKeyIdentifier(certificate),
    Buffer.from('bf37b3e5808fd46d54b28e846311bcce1cad2e1a', 'hex'),
  );
}

const leafKeys = generateKeyPairSync('rsa', { modulusLength: 2048 });
const rawValue = Buffer.from('deadbeefcafebabe', 'hex');
const leaf = generateX509CertificateSync(baseOptions(leafKeys, {
  issuer: ca,
  serialNumber: 2n,
  signature: {
    key: caKeys.privateKey,
    digest: 'sha256',
  },
  extensions: [
    { type: 'basicConstraints', ca: false },
    {
      type: 'keyUsage',
      usages: ['digitalSignature', 'keyEncipherment'],
      critical: true,
    },
    {
      type: 'extendedKeyUsage',
      usages: ['serverAuth', '1.2.3.5'],
    },
    {
      type: 'subjectAltName',
      names: [
        { type: 'dns', value: 'example.com' },
        { type: 'ip', value: '192.0.2.1' },
        { type: 'ip', value: '2001:db8::1' },
        { type: 'email', value: 'local@example.com' },
        { type: 'uri', value: 'https://example.com/path' },
      ],
    },
    { type: 'subjectKeyIdentifier', value: 'auto' },
    { type: 'authorityKeyIdentifier', value: 'auto' },
    { type: 'raw', oid: '1.2.3.8', critical: true, value: rawValue },
    { type: 'raw', oid: '2.5.29.18', value: Buffer.from('3000', 'hex') },
  ],
}));

assertSigned(leaf, caKeys.publicKey, leafKeys.privateKey);
assert.strictEqual(leaf.checkIssued(ca), true);
assert.strictEqual(leaf.issuerCertificate, undefined);
const clonedLeaf = structuredClone(leaf);
assert.deepStrictEqual(clonedLeaf.raw, leaf.raw);
assert.strictEqual(clonedLeaf.issuerCertificate, undefined);
assert.strictEqual(leaf.checkHost('example.com'), 'example.com');
assert.strictEqual(leaf.checkIP('192.0.2.1'), '192.0.2.1');
assert.strictEqual(leaf.checkIP('2001:db8::1'), '2001:db8::1');
assert.strictEqual(leaf.checkEmail('local@example.com'), 'local@example.com');
assert.match(leaf.subjectAltName, /URI:https:\/\/example\.com\/path/);
assert.deepStrictEqual(
  leaf.keyUsage, ['1.3.6.1.5.5.7.3.1', '1.2.3.5']);
assert(leaf.raw.includes(rawValue));
assert.deepStrictEqual(
  getAuthorityKeyIdentifier(leaf), getSubjectKeyIdentifier(ca));

// Binary typed values are identifier bytes rather than DER. BufferSource views
// are copied and wrapped by the crypto backend.
{
  const skiBacking = Uint8Array.from([0xff, 1, 2, 3, 4, 5, 0xff]);
  const akiBacking = Uint8Array.from([0xff, 6, 7, 8, 9, 10, 0xff]);
  const certificate = generateX509CertificateSync(baseOptions(leafKeys, {
    extensions: [
      {
        type: 'subjectKeyIdentifier',
        value: new DataView(skiBacking.buffer, 1, 5),
      },
      {
        type: 'authorityKeyIdentifier',
        value: new Uint8Array(akiBacking.buffer, 1, 5),
      },
    ],
  }));
  assert.deepStrictEqual(
    findExtension(certificate, subjectKeyIdentifierOid),
    {
      oid: subjectKeyIdentifierOid,
      critical: false,
      value: Buffer.from('04050102030405', 'hex'),
    },
  );
  assert.deepStrictEqual(
    findExtension(certificate, authorityKeyIdentifierOid),
    {
      oid: authorityKeyIdentifierOid,
      critical: false,
      value: Buffer.from('30078005060708090a', 'hex'),
    },
  );

  const copied = generateX509CertificateSync(baseOptions(leafKeys, {
    extensions: [
      { type: 'authorityKeyIdentifier', value: 'auto' },
      {
        type: 'subjectKeyIdentifier',
        value: new DataView(skiBacking.buffer, 1, 5),
      },
    ],
  }));
  assert.deepStrictEqual(
    findExtension(copied, authorityKeyIdentifierOid).value,
    Buffer.from('300780050102030405', 'hex'),
  );
}

// An automatic AKI copies an explicitly selected issuer SKI instead of
// replacing the issuer's identifier convention with RFC 7093 method 1.
{
  const identifier = Buffer.from('a1a2a3a4a5', 'hex');
  const issuer = generateX509CertificateSync(baseOptions(caKeys, {
    subject: [{ name: 'CN', value: 'Explicit identifier issuer' }],
    extensions: [{ type: 'subjectKeyIdentifier', value: identifier }],
  }));
  const certificate = generateX509CertificateSync(baseOptions(leafKeys, {
    issuer,
    signature: { key: caKeys.privateKey, digest: 'sha256' },
    extensions: [{ type: 'authorityKeyIdentifier', value: 'auto' }],
  }));
  assert.deepStrictEqual(
    findExtension(certificate, authorityKeyIdentifierOid).value,
    Buffer.concat([Buffer.from([0x30, 0x07, 0x80, identifier.length]),
                   identifier]),
  );

  const issuerWithoutIdentifier = generateX509CertificateSync(baseOptions(
    caKeys,
    {
      subject: [{ name: 'CN', value: 'Issuer without an identifier' }],
      extensions: [],
    },
  ));
  const derived = generateX509CertificateSync(baseOptions(leafKeys, {
    issuer: issuerWithoutIdentifier,
    signature: { key: caKeys.privateKey, digest: 'sha256' },
    extensions: [{ type: 'authorityKeyIdentifier', value: 'auto' }],
  }));
  assert.deepStrictEqual(
    getAuthorityKeyIdentifier(derived), getSubjectKeyIdentifier(ca));

  const malformedIssuer = generateX509CertificateSync(baseOptions(caKeys, {
    subject: [{ name: 'CN', value: 'Malformed identifier issuer' }],
    extensions: [{
      type: 'raw',
      oid: subjectKeyIdentifierOid,
      value: Buffer.from('0500', 'hex'),
    }],
  }));
  assertSynchronousFailure(baseOptions(leafKeys, {
    issuer: malformedIssuer,
    signature: { key: caKeys.privateKey, digest: 'sha256' },
    extensions: [{ type: 'authorityKeyIdentifier', value: 'auto' }],
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /issuer has an invalid Subject Key Identifier/,
  });
}

// Basic Constraints do not have to be critical merely because cA is true.
assertSigned(generateX509CertificateSync(baseOptions(caKeys, {
  extensions: [
    { type: 'basicConstraints', ca: true },
    { type: 'keyUsage', usages: ['keyCertSign'] },
  ],
})), caKeys.publicKey, caKeys.privateKey);

const emptySubject = generateX509CertificateSync(baseOptions(leafKeys, {
  subject: [],
  issuer: ca,
  signature: {
    key: caKeys.privateKey,
    digest: 'sha256',
  },
  extensions: [{
    type: 'subjectAltName',
    critical: true,
    names: [{ type: 'dns', value: 'empty-subject.example' }],
  }],
}));
assertSigned(emptySubject, caKeys.publicKey, leafKeys.privateKey);
assert.strictEqual(emptySubject.subject, '');
assert.strictEqual(emptySubject.issuer, ca.subject);

const encodedEmptySubject = generateX509CertificateSync(baseOptions(leafKeys, {
  subject: Buffer.from('3000', 'hex'),
  issuer: ca,
  signature: {
    key: caKeys.privateKey,
    digest: 'sha256',
  },
  extensions: [{
    type: 'subjectAltName',
    critical: true,
    names: [{ type: 'dns', value: 'empty-subject.example' }],
  }],
}));
assert.deepStrictEqual(
  encodedEmptySubject.subjectName.raw, Buffer.from('3000', 'hex'));

// DNS, email, and URI inputs receive encoding validation, not application
// syntax or policy validation.
assertSigned(generateX509CertificateSync(baseOptions(caKeys, {
  subject: [{ name: 'DC', value: 'not a domain label' }],
  extensions: [{
    type: 'subjectAltName',
    names: [
      { type: 'dns', value: 'not a dns name' },
      { type: 'email', value: 'not-an-email-address' },
      { type: 'uri', value: 'relative uri value' },
    ],
  }],
})), caKeys.publicKey, caKeys.privateKey);

for (const [type, options] of [
  ['ec', { namedCurve: 'P-256' }],
  ...hasFIPS() ? [] : [['ed25519', {}]],
]) {
  const keys = generateKeyPairSync(type, options);
  assertSigned(
    generateX509CertificateSync(baseOptions(keys)),
    keys.publicKey,
    keys.privateKey,
  );
}

{
  const keys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
  const passphrase = 'secret';
  const encryptedPrivateKey = keys.privateKey.export({
    type: 'pkcs8',
    format: 'pem',
    cipher: 'aes-256-cbc',
    passphrase,
  });
  const preparedKeys = [
    [
      keys.publicKey.export({ type: 'spki', format: 'pem' }),
      keys.privateKey.export({ type: 'pkcs8', format: 'pem' }),
    ],
    [
      {
        key: keys.publicKey.export({ type: 'spki', format: 'der' }),
        type: 'spki',
        format: 'der',
      },
      {
        key: keys.privateKey.export({ type: 'pkcs8', format: 'der' }),
        type: 'pkcs8',
        format: 'der',
      },
    ],
    [
      keys.privateKey,
      { key: encryptedPrivateKey, passphrase },
    ],
    [
      { key: keys.publicKey.export({ format: 'jwk' }), format: 'jwk' },
      { key: keys.privateKey.export({ format: 'jwk' }), format: 'jwk' },
    ],
  ];

  for (const [publicKey, key] of preparedKeys) {
    const certificate = generateX509CertificateSync(baseOptions(keys, {
      publicKey,
      signature: { key, digest: 'sha256' },
    }));
    assertSigned(certificate, keys.publicKey, keys.privateKey);
  }

  assertSynchronousFailure(baseOptions(keys, {
    publicKey: 'not a key',
  }), Error);
  assertSynchronousFailure(baseOptions(keys, {
    signature: { key: keys.publicKey, digest: 'sha256' },
  }), { code: 'ERR_CRYPTO_INVALID_KEY_OBJECT_TYPE' });
  assertSynchronousFailure(baseOptions(keys, {
    signature: {
      key: { key: encryptedPrivateKey, passphrase: 'wrong' },
      digest: 'sha256',
    },
  }), Error);
  assertSynchronousFailure(baseOptions(keys, {
    signature: {
      key: { key: keys.publicKey.export({ format: 'jwk' }), format: 'jwk' },
      digest: 'sha256',
    },
  }), { code: 'ERR_INVALID_ARG_VALUE' });
}

assertSigned(generateX509CertificateSync(baseOptions(leafKeys, {
  extensions: [{
    type: 'keyUsage',
    usages: ['encipherOnly', 'decipherOnly'],
  }],
})), leafKeys.publicKey, leafKeys.privateKey);

{
  const certificate = generateX509CertificateSync(baseOptions(leafKeys, {
    signature: {
      key: {
        key: leafKeys.privateKey,
        padding: constants.RSA_PKCS1_PSS_PADDING,
        saltLength: 32,
      },
      digest: 'sha256',
    },
  }));
  assertSigned(certificate, leafKeys.publicKey, leafKeys.privateKey);
  assert.strictEqual(certificate.signatureAlgorithmOid, '1.2.840.113549.1.1.10');
}

if (!isBoringSSL) {
  const keys = generateKeyPairSync('rsa-pss', {
    modulusLength: 2048,
    hashAlgorithm: 'sha256',
    mgf1HashAlgorithm: 'sha256',
    saltLength: 64,
  });
  const certificate = generateX509CertificateSync(baseOptions(keys));
  assertSigned(certificate, keys.publicKey, keys.privateKey);
  assert.strictEqual(certificate.signatureAlgorithmOid, '1.2.840.113549.1.1.10');
}

assert.strictEqual(generateX509Certificate.length, 2);
assert.strictEqual(generateX509CertificateSync.length, 1);

const invalidOptions = [
  ['ERR_OUT_OF_RANGE', baseOptions(leafKeys, { serialNumber: 0n })],
  ['ERR_OUT_OF_RANGE', baseOptions(leafKeys, { serialNumber: 1n << 159n })],
  ['ERR_OUT_OF_RANGE', baseOptions(leafKeys, {
    notBefore: new Date('1949-12-31T23:59:59Z'),
  })],
  ['ERR_OUT_OF_RANGE', baseOptions(leafKeys, {
    notAfter: new Date(253402300800000),
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    notBefore: new Date('2025-01-01T00:00:00.100Z'),
    notAfter: new Date('2025-01-01T00:00:00.900Z'),
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, { subject: [] })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    subject: Buffer.alloc(0),
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    subject: Buffer.from('3001', 'hex'),
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    subject: Buffer.from('3100', 'hex'),
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    subject: Buffer.concat([encodedSubject, Buffer.from([0])]),
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    subject: Buffer.from('3000', 'hex'),
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    subject: [],
    issuer: ca,
    signature: { key: caKeys.privateKey, digest: 'sha256' },
    extensions: [{
      type: 'subjectAltName',
      names: [{ type: 'dns', value: 'example.com' }],
    }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    subject: [],
    issuer: ca,
    signature: { key: caKeys.privateKey, digest: 'sha256' },
    extensions: [{
      type: 'raw',
      oid: '2.5.29.17',
      critical: true,
      value: Buffer.from('300d820b6578616d706c652e636f6d', 'hex'),
    }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    subject: [{ name: 'C', value: 'CZE' }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    subject: [{ name: '2.5.4.6', value: 'CZE' }],
  })],
  ['ERR_MISSING_OPTION', baseOptions(leafKeys, {
    subject: [{ oid: '1.2.3.4', raw: Buffer.from('0c00', 'hex') }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [{ type: 'keyUsage', usages: ['keyCertSign'] }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [{ type: 'basicConstraints', ca: true, pathLength: 0 }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [
      { type: 'basicConstraints', ca: false },
      {
        type: 'raw',
        oid: '2.5.29.19',
        value: Buffer.from('3000', 'hex'),
      },
    ],
  })],
  ['ERR_INVALID_ARG_TYPE', baseOptions(leafKeys, {
    extensions: [{ type: 'raw', oid: '1.2.3.4', value: '0500' }],
  })],
  ['ERR_MISSING_OPTION', baseOptions(leafKeys, {
    extensions: [{ type: 'subjectKeyIdentifier' }],
  })],
  ['ERR_INVALID_ARG_TYPE', baseOptions(leafKeys, {
    extensions: [{ type: 'authorityKeyIdentifier', value: 'sha256' }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [
      { type: 'subjectKeyIdentifier', value: 'auto' },
      {
        type: 'raw',
        oid: subjectKeyIdentifierOid,
        value: Buffer.from('040101', 'hex'),
      },
    ],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [
      {
        type: 'raw',
        oid: authorityKeyIdentifierOid,
        value: Buffer.from('3003800101', 'hex'),
      },
      { type: 'authorityKeyIdentifier', value: 'auto' },
    ],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [{
      type: 'subjectAltName',
      names: [{ type: 'directoryName', value: [] }],
    }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [{
      type: 'subjectAltName',
      names: [{ type: 'dns', value: 'b\u00fccher.example' }],
    }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [{
      type: 'subjectAltName',
      names: [{ type: 'dns', value: 'nul\0.example' }],
    }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [{
      type: 'subjectAltName',
      names: [{ type: 'dns', value: '' }],
    }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [{
      type: 'subjectAltName',
      names: [{ type: 'email', value: '' }],
    }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [{
      type: 'subjectAltName',
      names: [{ type: 'uri', value: '' }],
    }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [{
      type: 'subjectAltName',
      names: [{ type: 'ip', value: '192.0.2.999' }],
    }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    extensions: [{
      type: 'subjectAltName',
      names: [{ type: 'ip', value: 'fe80::1%eth0' }],
    }],
  })],
  ['ERR_INVALID_ARG_VALUE', baseOptions(leafKeys, {
    signature: {
      key: leafKeys.privateKey,
      digest: null,
    },
  })],
];

for (const [code, options] of invalidOptions) {
  assert.throws(() => generateX509CertificateSync(options), { code });
}

assert.throws(() => generateX509CertificateSync(baseOptions(leafKeys, {
  subject: [],
  extensions: [{
    type: 'subjectAltName',
    critical: true,
    names: [{ type: 'dns', value: 'self-signed.example' }],
  }],
})), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /self-signed X\.509 certificate must have a non-empty subject/,
});

assert.throws(() => generateX509CertificateSync(baseOptions(leafKeys, {
  issuer: emptySubjectIssuer,
})), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /"options\.issuer" certificate must have a non-empty subject/,
});

assert.throws(() => generateX509Certificate({}, undefined), {
  code: 'ERR_INVALID_ARG_TYPE',
});

assert.throws(() => generateX509Certificate(baseOptions(leafKeys, {
  extensions: [
    { type: 'basicConstraints', ca: false },
    { type: 'basicConstraints', ca: false },
  ],
}), common.mustNotCall()), {
  code: 'ERR_INVALID_ARG_VALUE',
});

for (const [oid, value] of [
  ['2.5.29.19', Buffer.from('3000', 'hex')],
  ['2.5.29.14', Buffer.from('0400', 'hex')],
  ['2.5.29.35', Buffer.from('3001', 'hex')],
  ['2.5.29.35', Buffer.from('30000500', 'hex')],
]) {
  const options = baseOptions(leafKeys, {
    issuer: ca,
    signature: {
      key: caKeys.privateKey,
      digest: 'sha256',
    },
    extensions: [{
      type: 'raw',
      oid,
      value,
    }],
  });
  const backendMayReject = value.length === 2 &&
    (oid === '2.5.29.14' || oid === '2.5.29.35');
  let certificate;
  try {
    certificate = generateX509CertificateSync(options);
  } catch (error) {
    if (!backendMayReject) throw error;
    assert.strictEqual(error.code, 'ERR_CRYPTO_UNSUPPORTED_OPERATION');
    assert.throws(() => generateX509Certificate(
      options, common.mustNotCall()), {
      code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION',
    });
    continue;
  }
  assertSigned(certificate, caKeys.publicKey, leafKeys.privateKey);
  assert.deepStrictEqual(certificate.extensions, [{
    oid,
    critical: false,
    value,
  }]);
}

const mismatchedKeys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
assert.throws(() => generateX509Certificate(
  baseOptions(leafKeys, {
    signature: {
      key: mismatchedKeys.privateKey,
      digest: 'sha256',
    },
  }),
  common.mustNotCall(),
), {
  code: 'ERR_INVALID_ARG_VALUE',
});

(async () => {
  const mutableName = { name: 'CN', value: 'before.example.com' };
  const expectedRaw = Buffer.from('0408feedfacedeadbeef', 'hex');
  const mutableRaw = Buffer.from(expectedRaw);
  const expectedIdentifier = Buffer.from('0102030405', 'hex');
  const mutableIdentifier = Buffer.from(expectedIdentifier);
  const options = baseOptions(leafKeys, {
    subject: [mutableName],
    issuer: ca,
    signature: {
      key: caKeys.privateKey,
      digest: 'sha256',
    },
    extensions: [
      { type: 'raw', oid: '1.2.3.9', value: mutableRaw },
      { type: 'subjectKeyIdentifier', value: mutableIdentifier },
    ],
  });
  const pending = generateX509CertificateAsync(options);
  mutableName.value = 'after.example.com';
  mutableRaw.fill(0);
  mutableIdentifier.fill(0);
  options.extensions.length = 0;
  const generated = await pending;
  assert.strictEqual(generated.subject, 'CN=before.example.com');
  assert(generated.raw.includes(expectedRaw));
  assert.deepStrictEqual(
    findExtension(generated, subjectKeyIdentifierOid).value,
    Buffer.concat([Buffer.from([0x04, expectedIdentifier.length]),
                   expectedIdentifier]),
  );
  assert.strictEqual(generated.issuerCertificate, undefined);

  const mutableSubject = Buffer.from(encodedSubject);
  const encodedSubjectPending = generateX509CertificateAsync(baseOptions(
    leafKeys,
    { subject: mutableSubject },
  ));
  mutableSubject.fill(0);
  const encodedSubjectCertificate = await encodedSubjectPending;
  assert.deepStrictEqual(
    encodedSubjectCertificate.subjectName.raw, encodedSubject);

  const encodedKeys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
  const publicKey = encodedKeys.publicKey.export({
    type: 'spki',
    format: 'der',
  });
  const privateKey = encodedKeys.privateKey.export({
    type: 'pkcs8',
    format: 'der',
  });
  const encodedKeyPending = generateX509CertificateAsync(baseOptions(
    encodedKeys,
    {
      publicKey: { key: publicKey, type: 'spki', format: 'der' },
      signature: {
        key: { key: privateKey, type: 'pkcs8', format: 'der' },
        digest: 'sha256',
      },
    },
  ));
  publicKey.fill(0);
  privateKey.fill(0);
  const encodedKeyCertificate = await encodedKeyPending;
  assertSigned(
    encodedKeyCertificate,
    encodedKeys.publicKey,
    encodedKeys.privateKey,
  );

  const callbackResult = await new Promise((resolve, reject) => {
    generateX509Certificate(baseOptions(leafKeys, {
      issuer: ca,
      signature: {
        key: caKeys.privateKey,
        digest: 'sha256',
      },
    }), (error, certificate) => {
      if (error) return reject(error);
      resolve(certificate);
    });
  });
  assertSigned(callbackResult, caKeys.publicKey, leafKeys.privateKey);
  assert.strictEqual(callbackResult.issuerCertificate, undefined);

  if (!hasFIPS()) {
    const unsupportedKeys = generateKeyPairSync('x25519');
    await new Promise((resolve) => {
      generateX509Certificate(baseOptions(unsupportedKeys), common.mustCall(
        (error, certificate) => {
          assert(error instanceof Error);
          assert.strictEqual(certificate, undefined);
          resolve();
        }));
    });
  }

  const esm = await import('node:crypto');
  assert.strictEqual(esm.generateX509Certificate, generateX509Certificate);
  assert.strictEqual(
    esm.generateX509CertificateSync, generateX509CertificateSync);
})().then(common.mustCall());

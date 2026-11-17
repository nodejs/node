'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { hasFIPS } = require('../common/crypto');
const {
  createPrivateKey,
  generateKeyPairSync,
  generateX509Certificate,
  generateX509CertificateSync,
  X509Certificate,
} = require('crypto');
const fixtures = require('../common/fixtures');

const csr = `-----BEGIN CERTIFICATE REQUEST-----
MIIBOzCB4wIBADAuMRAwDgYDVQQKDAdOb2RlLmpzMRowGAYDVQQDDBFyZXF1ZXN0
ZWQuZXhhbXBsZTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEppwUcxZQc2qbTx
ko8VEYAtKhMPbzzkhMlC3iqv+DJG9gXRJ3Kt8vz3/HuBV6gGDjuaTObou//p7sBj
OdvcTXCgUzBRBgkqhkiG9w0BCQ4xRDBCMAwGA1UdEwEB/wQCMAAwDgYDVR0PAQH/
BAQDAgOIMCIGA1UdEQQbMBmCEXJlcXVlc3RlZC5leGFtcGxlhwTAAAIBMAoGCCqG
SM49BAMCA0cAMEQCIBYr+BErErRvcUIOPoAORKKS2x4mPpLPKlWNU1MxItrvAiAg
0eXrZcLTeGhOpUB3hkIdfASAgqOp+sHKWLj5nBr/5A==
-----END CERTIFICATE REQUEST-----`;
const csrDer = Buffer.from(
  csr.split('\n').slice(1, -1).join(''),
  'base64',
);
const ed25519Csr = `-----BEGIN CERTIFICATE REQUEST-----
MIGhMFUCAQAwIjEgMB4GA1UEAwwXZWQyNTUxOS1yZXF1ZXN0LmV4YW1wbGUwKjAF
BgMrZXADIQArXAii6qe6IDThvdasxr7Eogoil8d601lG/eH18F/vr6AAMAUGAytl
cANBACDLbdXbIje6mUPTvgtLtdUlWBV39VwDlq4jwoYm09XgriyJKSh4vt7RUuhI
7gjQGMN+Thmaas/5esSZYZ5ZmQQ=
-----END CERTIFICATE REQUEST-----`;
const emptySubjectCsr = `-----BEGIN CERTIFICATE REQUEST-----
MIHwMIGYAgEAMAAwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARKacFHMWUHNqm0
8ZKPFRGALSoTD2885ITJQt4qr/gyRvYF0SdyrfL89/x7gVeoBg47mkzm6Lv/6e7A
Yznb3E1woDYwNAYJKoZIhvcNAQkOMScwJTAjBgNVHREBAf8EGTAXghVlbXB0eS1z
dWJqZWN0LmV4YW1wbGUwCgYIKoZIzj0EAwIDRwAwRAIgCrO1gUwniOWwi99FaB8X
lFGt950G2N/Yaa2mv5uV3qsCIB4S7xXaiS4GcwwAgrsFrmXOb+Wq3jx+HNoS0O21
kK/d
-----END CERTIFICATE REQUEST-----`;
const privateKeyPem = fixtures.readKey('ec-key.pem');
const privateKey = createPrivateKey(privateKeyPem);
const notBefore = new Date('2025-01-02T03:04:05Z');
const notAfter = new Date('2030-01-02T03:04:05Z');

const requestedExtensions = [
  {
    type: 'raw',
    oid: '2.5.29.19',
    critical: true,
    value: Buffer.from('3000', 'hex'),
  },
  {
    type: 'raw',
    oid: '2.5.29.15',
    critical: true,
    value: Buffer.from('03020388', 'hex'),
  },
  {
    type: 'raw',
    oid: '2.5.29.17',
    critical: false,
    value: Buffer.from(
      '301982117265717565737465642e6578616d706c658704c0000201',
      'hex',
    ),
  },
];

function options(
  csrInput = csr,
  selectExtensions = (requested) => requested,
) {
  return {
    csr: csrInput,
    serialNumber: 1n,
    notBefore,
    notAfter,
    signature: {
      key: privateKeyPem,
      digest: 'sha256',
    },
    selectExtensions,
  };
}

function assertRequestCertificate(certificate) {
  assert(certificate instanceof X509Certificate);
  assert.strictEqual(certificate.subject, 'O=Node.js\nCN=requested.example');
  assert.strictEqual(certificate.checkPrivateKey(privateKey), true);
}

const padded = Buffer.concat([Buffer.alloc(7), csrDer, Buffer.alloc(5)]);
const offsetView = new Uint8Array(
  padded.buffer,
  padded.byteOffset + 7,
  csrDer.length,
);
const sharedCsr = new SharedArrayBuffer(csrDer.length);
new Uint8Array(sharedCsr).set(csrDer);
const arrayBufferCsr = csrDer.buffer.slice(
  csrDer.byteOffset,
  csrDer.byteOffset + csrDer.byteLength,
);
for (const input of [
  csr,
  Buffer.from(csr),
  csrDer,
  offsetView,
  arrayBufferCsr,
  new DataView(arrayBufferCsr),
  sharedCsr,
  `ignored by the backend parser\n${csr}\r\n`,
]) {
  const certificate = generateX509CertificateSync(options(input, common.mustCall((requested) => {
    assert.deepStrictEqual(requested, requestedExtensions);
    return requested;
  })));
  assertRequestCertificate(certificate);
  assert.deepStrictEqual(
    certificate.extensions,
    requestedExtensions.map(({ type, ...extension }) => extension),
  );
}

{
  const certificate = generateX509CertificateSync(options(csr, () => [
    { type: 'basicConstraints', ca: false },
    {
      type: 'subjectAltName',
      names: [{ type: 'dns', value: 'policy.example' }],
    },
  ]));
  assertRequestCertificate(certificate);
  assert.strictEqual(certificate.checkHost('policy.example'), 'policy.example');
  assert.strictEqual(certificate.checkHost('requested.example'), undefined);
}

{
  const certificate = generateX509CertificateSync(options(csr, () => [
    { type: 'subjectKeyIdentifier', value: 'auto' },
    { type: 'authorityKeyIdentifier', value: 'auto' },
  ]));
  assertRequestCertificate(certificate);
  assert.deepStrictEqual(
    certificate.extensions.map(({ oid }) => oid),
    ['2.5.29.14', '2.5.29.35'],
  );
  assert.deepStrictEqual(
    certificate.extensions[0].value.subarray(2),
    certificate.extensions[1].value.subarray(4),
  );
}

{
  const requestWithoutExtensions = fixtures.readKey('ec-csr.pem', 'utf8');
  let requested;
  const certificate = generateX509CertificateSync(options(
    requestWithoutExtensions,
    (value) => {
      requested = value;
      return [];
    },
  ));
  assert.deepStrictEqual(requested, []);
  assert.strictEqual(certificate.checkPrivateKey(privateKey), true);
}

{
  const rsaCsr = fixtures.readKey('agent1-csr.pem', 'utf8');
  const rsaKey = fixtures.readKey('agent1-key.pem');
  const certificate = generateX509CertificateSync({
    ...options(rsaCsr, () => []),
    signature: { key: rsaKey, digest: 'sha256' },
  });
  assert.strictEqual(certificate.checkPrivateKey(createPrivateKey(rsaKey)), true);
}

if (!hasFIPS()) {
  const ed25519Key = fixtures.readKey('ed25519_private.pem');
  const certificate = generateX509CertificateSync({
    ...options(ed25519Csr, () => []),
    signature: { key: ed25519Key, digest: null },
  });
  assert.strictEqual(certificate.subject, 'CN=ed25519-request.example');
  assert.strictEqual(
    certificate.checkPrivateKey(createPrivateKey(ed25519Key)),
    true,
  );
}

{
  const issuerKeys = generateKeyPairSync('ec', { namedCurve: 'P-256' });
  const issuer = generateX509CertificateSync({
    subject: [{ name: 'CN', value: 'CSR issuer' }],
    publicKey: issuerKeys.publicKey,
    serialNumber: 2n,
    notBefore,
    notAfter,
    signature: { key: issuerKeys.privateKey, digest: 'sha256' },
  });
  const certificate = generateX509CertificateSync({
    ...options(csr, () => []),
    issuer,
    signature: { key: issuerKeys.privateKey, digest: 'sha256' },
  });
  assert.strictEqual(certificate.verify(issuerKeys.publicKey), true);
  assert.strictEqual(certificate.checkPrivateKey(privateKey), true);
  assert.strictEqual(certificate.checkIssued(issuer), true);

  assert.throws(() => generateX509CertificateSync({
    ...options(emptySubjectCsr),
    issuer,
    signature: { key: issuerKeys.privateKey, digest: 'sha256' },
  }), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /extension profile/,
  });
  const emptySubjectCertificate = generateX509CertificateSync({
    ...options(emptySubjectCsr, () => [{
      type: 'subjectAltName',
      critical: true,
      names: [{ type: 'dns', value: 'empty-subject.example' }],
    }]),
    issuer,
    signature: { key: issuerKeys.privateKey, digest: 'sha256' },
  });
  assert.strictEqual(emptySubjectCertificate.subject, '');
  assert.strictEqual(
    emptySubjectCertificate.checkHost('empty-subject.example'),
    'empty-subject.example',
  );
}

for (const conflict of [
  { subject: [{ name: 'CN', value: 'ignored' }] },
  { publicKey: privateKey },
  { extensions: [] },
]) {
  assert.throws(
    () => generateX509CertificateSync({ ...options(), ...conflict }),
    { code: 'ERR_INCOMPATIBLE_OPTION_PAIR' },
  );
}

assert.throws(() => generateX509CertificateSync({
  selectExtensions: common.mustNotCall(),
}), {
  code: 'ERR_MISSING_OPTION',
  message: /options\.csr/,
});

{
  const missing = options();
  delete missing.selectExtensions;
  assert.throws(() => generateX509CertificateSync(missing), {
    code: 'ERR_MISSING_OPTION',
    message: /options\.selectExtensions/,
  });
}

for (const selectExtensions of [
  [],
  undefined,
  () => undefined,
  () => Promise.resolve([]),
]) {
  const value = options(csr, selectExtensions);
  if (selectExtensions === undefined) delete value.selectExtensions;
  assert.throws(() => generateX509CertificateSync(value), {
    code: selectExtensions === undefined ? 'ERR_MISSING_OPTION' :
      'ERR_INVALID_ARG_TYPE',
    message: /options\.selectExtensions/,
  });
}

assert.throws(
  () => generateX509CertificateSync(options(csr, () => [{}])),
  {
    code: 'ERR_MISSING_OPTION',
    message: /options\.selectExtensions return value\[0\]\.type/,
  },
);

{
  const expected = new Error('policy rejected the request');
  assert.throws(() => generateX509Certificate(
    options(csr, () => { throw expected; }),
    common.mustNotCall(),
  ), (error) => error === expected);
}

const invalidExtensionValue = Buffer.from(csrDer);
const extensionRequest = invalidExtensionValue.indexOf(
  Buffer.from('31443042', 'hex'),
);
assert.notStrictEqual(extensionRequest, -1);
invalidExtensionValue[extensionRequest + 2] = 0x04;

const invalidVersion = Buffer.from(csrDer);
invalidVersion[9] = 1;

const duplicateRequestedOid = Buffer.from(csrDer);
const subjectAltNameOid = duplicateRequestedOid.indexOf(
  Buffer.from('0603551d11', 'hex'),
);
assert.notStrictEqual(subjectAltNameOid, -1);
duplicateRequestedOid[subjectAltNameOid + 4] = 0x13;

const duplicateExtensionRequests = Buffer.from(
  'MIIBkDCCATcCAQAwLjEQMA4GA1UECgwHTm9kZS5qczEaMBgGA1UEAwwRcmVx' +
  'dWVzdGVkLmV4YW1wbGUwWTATBgcqhkjOPQIBBggqhkjOPQMBBwNCAARKacFH' +
  'MWUHNqm08ZKPFRGALSoTD2885ITJQt4qr/gyRvYF0SdyrfL89/x7gVeoBg47' +
  'mkzm6Lv/6e7AYznb3E1woIGmMFEGCSqGSIb3DQEJDjFEMEIwDAYDVR0TAQH/' +
  'BAIwADAOBgNVHQ8BAf8EBAMCA4gwIgYDVR0RBBswGYIRcmVxdWVzdGVkLmV4' +
  'YW1wbGWHBMAAAgEwUQYJKoZIhvcNAQkOMUQwQjAMBgNVHRMBAf8EAjAAMA4G' +
  'A1UdDwEB/wQEAwIDiDAiBgNVHREEGzAZghFyZXF1ZXN0ZWQuZXhhbXBsZYcE' +
  'wAACATAKBggqhkjOPQQDAgNHADBEAiAWK/gRKxK0b3FCDj6ADkSiktseJj6S' +
  'zypVjVNTMSLa7wIgINHl62XC03hoTqVAd4ZCHXwEgIKjqfrByli4+Zwa/+Q=',
  'base64',
);
assert.strictEqual(duplicateExtensionRequests.length, 404);

const pemWithDecodedSuffix =
  `-----BEGIN CERTIFICATE REQUEST-----\n${Buffer.concat([
    csrDer,
    Buffer.from('suffix'),
  ]).toString('base64')}\n-----END CERTIFICATE REQUEST-----`;

// CSR inputs follow the crypto backend's parsing behavior. As with existing
// certificate inputs, bytes after the first decoded object are not rejected.
for (const input of [
  Buffer.concat([csrDer, Buffer.from('suffix')]),
  `${csr}\n${csr}`,
  pemWithDecodedSuffix,
]) {
  assertRequestCertificate(generateX509CertificateSync(options(input)));
}

// When more than one recognized extension-request attribute is present, use
// the list selected by the crypto backend.
assert.throws(() => generateX509CertificateSync(options(
  duplicateExtensionRequests,
  common.mustCall((requested) => {
    assert.deepStrictEqual(requested, requestedExtensions);
    return requested;
  }),
)), {
  code: 'ERR_CRYPTO_OPERATION_FAILED',
  message: /Certificate request signature verification failed/,
});

for (const invalid of [
  Buffer.from([0x30, 0x00]),
  invalidVersion,
  invalidExtensionValue,
  csr.replace('REQUEST-----\n', 'REQUEST-----junk\n'),
]) {
  assert.throws(() => generateX509CertificateSync(
    options(invalid, common.mustNotCall()),
  ), {
    code: 'ERR_INVALID_ARG_VALUE',
    message: /options\.csr/,
  });
}
assert.throws(() => generateX509Certificate(
  options(invalidExtensionValue, common.mustNotCall()),
  common.mustNotCall(),
), {
  code: 'ERR_INVALID_ARG_VALUE',
  message: /options\.csr/,
});

// Requested extensions are untrusted policy input. Even conflicting requests
// reach the policy callback so that it can choose what to include. The modified
// request no longer has a valid proof of possession, which is checked only
// after policy has run.
assert.throws(() => generateX509CertificateSync(
  options(duplicateRequestedOid, common.mustCall((requested) => {
    assert.strictEqual(requested[0].oid, requested[2].oid);
    return [requested[0]];
  })),
), {
  code: 'ERR_CRYPTO_OPERATION_FAILED',
  message: /Certificate request signature verification failed/,
});

const invalidProof = Buffer.from(csrDer);
invalidProof[invalidProof.length - 1] ^= 1;
let invalidProofPolicyCalls = 0;
assert.throws(() => generateX509CertificateSync(options(
  invalidProof,
  () => {
    invalidProofPolicyCalls++;
    return [];
  },
)), {
  code: 'ERR_CRYPTO_OPERATION_FAILED',
  message: /Certificate request signature verification failed/,
});
assert.strictEqual(invalidProofPolicyCalls, 1);

(async () => {
  let returned = false;
  let policyInput;
  const mutableCsr = Buffer.from(csrDer);
  const generated = await new Promise((resolve, reject) => {
    generateX509Certificate(options(mutableCsr, common.mustCall((requested) => {
      assert.strictEqual(returned, false);
      policyInput = requested;
      return requested;
    })), common.mustCall((error, certificate) => {
      assert.strictEqual(returned, true);
      if (error) return reject(error);
      resolve(certificate);
    }));
    returned = true;
    mutableCsr.fill(0);
    policyInput[0].value.fill(0);
  });
  assertRequestCertificate(generated);
  assert.deepStrictEqual(
    generated.extensions[0].value,
    requestedExtensions[0].value,
  );

  await new Promise((resolve) => {
    generateX509Certificate(options(invalidProof, () => []), common.mustCall(
      (error, certificate) => {
        assert.strictEqual(error.code, 'ERR_CRYPTO_OPERATION_FAILED');
        assert.match(
          error.message,
          /Certificate request signature verification failed/,
        );
        assert.strictEqual(certificate, undefined);
        resolve();
      },
    ));
  });
})().then(common.mustCall());

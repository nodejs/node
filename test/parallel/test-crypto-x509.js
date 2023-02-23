// Flags: --expose-internals
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const {
  X509Certificate,
  createPrivateKey,
  generateKeyPairSync,
  createSign,
} = require('crypto');

const {
  isX509Certificate
} = require('internal/crypto/x509');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const { readFileSync } = require('fs');

const cert = readFileSync(fixtures.path('keys', 'agent1-cert.pem'));
const key = readFileSync(fixtures.path('keys', 'agent1-key.pem'));
const ca = readFileSync(fixtures.path('keys', 'ca1-cert.pem'));

const privateKey = createPrivateKey(key);

[1, {}, false, null].forEach((i) => {
  assert.throws(() => new X509Certificate(i), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

const subjectCheck = `C=US
ST=CA
L=SF
O=Joyent
OU=Node.js
CN=agent1
emailAddress=ry@tinyclouds.org`;

const issuerCheck = `C=US
ST=CA
L=SF
O=Joyent
OU=Node.js
CN=ca1
emailAddress=ry@tinyclouds.org`;

let infoAccessCheck = `OCSP - URI:http://ocsp.nodejs.org/
CA Issuers - URI:http://ca.nodejs.org/ca.cert`;
if (!common.hasOpenSSL3)
  infoAccessCheck += '\n';

const der = Buffer.from(
  '308202d830820241a003020102020900ecc9b856270da9a830' +
  '0d06092a864886f70d01010b0500307a310b30090603550406' +
  '13025553310b300906035504080c024341310b300906035504' +
  '070c025346310f300d060355040a0c064a6f79656e74311030' +
  '0e060355040b0c074e6f64652e6a73310c300a06035504030c' +
  '036361313120301e06092a864886f70d010901161172794074' +
  '696e79636c6f7564732e6f72673020170d3138313131363138' +
  '343232315a180f32323932303833303138343232315a307d31' +
  '0b3009060355040613025553310b300906035504080c024341' +
  '310b300906035504070c025346310f300d060355040a0c064a' +
  '6f79656e743110300e060355040b0c074e6f64652e6a73310f' +
  '300d06035504030c066167656e74313120301e06092a864886' +
  'f70d010901161172794074696e79636c6f7564732e6f726730' +
  '819f300d06092a864886f70d010101050003818d0030818902' +
  '818100ef5440701637e28abb038e5641f828d834c342a9d25e' +
  'dbb86a2bf6fbd809cb8e037a98b71708e001242e4deb54c616' +
  '4885f599dd87a23215745955be20417e33c4d0d1b80c9da3de' +
  '419a2607195d2fb75657b0bbfb5eb7d0bba5122d1b6964c7b5' +
  '70d50b8ec001eeb68dfb584437508f3129928d673b30a3e0bf' +
  '4f50609e63710203010001a361305f305d06082b0601050507' +
  '01010451304f302306082b060105050730018617687474703a' +
  '2f2f6f6373702e6e6f64656a732e6f72672f302806082b0601' +
  '0505073002861c687474703a2f2f63612e6e6f64656a732e6f' +
  '72672f63612e63657274300d06092a864886f70d01010b0500' +
  '038181007acabf1d99e1fb05edbdd54608886dd6c509fc5820' +
  '2be8274f8139b60f8ea219666f7eff9737e92a732b318ef423' +
  '7da94123dcac4f9a28e76fe663b26d42482ac6d66d380bbdfe' +
  '0230083e743e7966671752b82f692e1034e9bfc9d0cd829888' +
  '6c6c996e7c3d231e02ad5399a170b525b74f11d7ed13a7a815' +
  'f4b974253a8d66', 'hex');

{
  const x509 = new X509Certificate(cert);

  assert(isX509Certificate(x509));

  assert(!x509.ca);
  assert.strictEqual(x509.subject, subjectCheck);
  assert.strictEqual(x509.subjectAltName, undefined);
  assert.strictEqual(x509.issuer, issuerCheck);
  assert.strictEqual(x509.infoAccess, infoAccessCheck);
  assert.strictEqual(x509.validFrom, 'Nov 16 18:42:21 2018 GMT');
  assert.strictEqual(x509.validTo, 'Aug 30 18:42:21 2292 GMT');
  assert.strictEqual(
    x509.fingerprint,
    'D7:FD:F6:42:92:A8:83:51:8E:80:48:62:66:DA:85:C2:EE:A6:A1:CD');
  assert.strictEqual(
    x509.fingerprint256,
    'B0:BE:46:49:B8:29:63:E0:6F:63:C8:8A:57:9C:3F:9B:72:C6:F5:89:E3:0D:' +
    '84:AC:5B:08:9A:20:89:B6:8F:D6'
  );
  assert.strictEqual(
    x509.fingerprint512,
    'D0:05:01:82:2C:D8:09:BE:27:94:E7:83:F1:88:BC:7A:8B:D0:39:97:54:B6:' +
    'D0:B4:46:5B:DE:13:5B:68:86:B6:F2:A8:95:22:D5:6E:8B:35:DA:89:29:CA:' +
    'A3:06:C5:CE:43:C1:7F:2D:7E:5F:44:A5:EE:A3:CB:97:05:A3:E3:68'
  );
  assert.strictEqual(x509.keyUsage, undefined);
  assert.strictEqual(x509.serialNumber, 'ECC9B856270DA9A8');

  assert.deepStrictEqual(x509.raw, der);

  assert(x509.publicKey);
  assert.strictEqual(x509.publicKey.type, 'public');

  assert.strictEqual(x509.toString().replaceAll('\r\n', '\n'),
                     cert.toString().replaceAll('\r\n', '\n'));
  assert.strictEqual(x509.toJSON(), x509.toString());

  assert(x509.checkPrivateKey(privateKey));
  assert.throws(() => x509.checkPrivateKey(x509.publicKey), {
    code: 'ERR_INVALID_ARG_VALUE'
  });

  assert.strictEqual(x509.checkIP('127.0.0.1'), undefined);
  assert.strictEqual(x509.checkIP('::'), undefined);
  assert.strictEqual(x509.checkHost('agent1'), 'agent1');
  assert.strictEqual(x509.checkHost('agent2'), undefined);
  assert.strictEqual(x509.checkEmail('ry@tinyclouds.org'), 'ry@tinyclouds.org');
  assert.strictEqual(x509.checkEmail('sally@example.com'), undefined);
  assert.throws(() => x509.checkHost('agent\x001'), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
  assert.throws(() => x509.checkIP('[::]'), {
    code: 'ERR_INVALID_ARG_VALUE'
  });
  assert.throws(() => x509.checkEmail('not\x00hing'), {
    code: 'ERR_INVALID_ARG_VALUE'
  });

  [1, false, null].forEach((i) => {
    assert.throws(() => x509.checkHost('agent1', i), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
    assert.throws(() => x509.checkHost('agent1', { subject: i }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  [
    'wildcards',
    'partialWildcards',
    'multiLabelWildcards',
    'singleLabelSubdomains',
  ].forEach((key) => {
    [1, '', null, {}].forEach((i) => {
      assert.throws(() => x509.checkHost('agent1', { [key]: i }), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    });
  });

  const ca_cert = new X509Certificate(ca);

  assert(x509.checkIssued(ca_cert));
  assert(!x509.checkIssued(x509));
  assert(x509.verify(ca_cert.publicKey));
  assert(!x509.verify(x509.publicKey));

  assert.throws(() => x509.checkIssued({}), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => x509.checkIssued(''), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => x509.verify({}), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => x509.verify(''), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
  assert.throws(() => x509.verify(privateKey), {
    code: 'ERR_INVALID_ARG_VALUE'
  });

  {
    // https://github.com/nodejs/node/issues/45377
    // https://github.com/nodejs/node/issues/45485
    // Confirm failures of
    // X509Certificate:verify()
    // X509Certificate:CheckPrivateKey()
    // X509Certificate:CheckCA()
    // X509Certificate:CheckIssued()
    // X509Certificate:ToLegacy()
    // do not affect other functions that use OpenSSL.
    // Subsequent calls to e.g. createPrivateKey should not throw.
    const keyPair = generateKeyPairSync('ed25519');
    assert(!x509.verify(keyPair.publicKey));
    createPrivateKey(key);
    assert(!x509.checkPrivateKey(keyPair.privateKey));
    createPrivateKey(key);
    const certPem = `
-----BEGIN CERTIFICATE-----
MIID6zCCAtOgAwIBAgIUTUREAaNcNL0zPkxAlMX0GJtJ/FcwDQYJKoZIhvcNAQEN
BQAwgYkxCzAJBgNVBAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMREwDwYDVQQH
DAhDYXJsc2JhZDEPMA0GA1UECgwGVmlhc2F0MR0wGwYDVQQLDBRWaWFzYXQgU2Vj
dXJlIE1vYmlsZTEiMCAGA1UEAwwZSGFja2VyT25lIHJlcG9ydCAjMTgwODU5NjAi
GA8yMDIyMTIxNjAwMDAwMFoYDzIwMjMxMjE1MjM1OTU5WjCBiTELMAkGA1UEBhMC
VVMxEzARBgNVBAgMCkNhbGlmb3JuaWExETAPBgNVBAcMCENhcmxzYmFkMQ8wDQYD
VQQKDAZWaWFzYXQxHTAbBgNVBAsMFFZpYXNhdCBTZWN1cmUgTW9iaWxlMSIwIAYD
VQQDDBlIYWNrZXJPbmUgcmVwb3J0ICMxODA4NTk2MIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEA6I7RBPm4E/9rIrCHV5lfsHI/yYzXtACJmoyP8OMkjbeB
h21oSJJF9FEnbivk6bYaHZIPasa+lSAydRM2rbbmfhF+jQoWYCIbV2ztrbFR70S1
wAuJrlYYm+8u+1HUru5UBZWUr/p1gFtv3QjpA8+43iwE4pXytTBKPXFo1f5iZwGI
D5Bz6DohT7Tyb8cpQ1uMCMCT0EJJ4n8wUrvfBgwBO94O4qlhs9vYgnDKepJDjptc
uSuEpvHALO8+EYkQ7nkM4Xzl/WK1yFtxxE93Jvd1OvViDGVrRVfsq+xYTKknGLX0
QIeoDDnIr0OjlYPd/cqyEgMcFyFxwDSzSc1esxdCpQIDAQABo0UwQzAdBgNVHQ4E
FgQUurygsEKdtQk0T+sjM0gEURdveRUwEgYDVR0TAQH/BAgwBgEB/wIB/zAOBgNV
HQ8BAf8EBAMCB4AwDQYJKoZIhvcNAQENBQADggEBAH7mIIXiQsQ4/QGNNFOQzTgP
/bUbMSZJsY5TPAvS9rF9yQVzs4dJZnQk5kEb/qrDQSe27oP0L0hfFm1wTGy+aKfa
BVGHdRmmvHtDUPLA9URCFShqKuS+GXp+6zt7dyZPRrPmiZaciiCMPHOnx59xSdPm
AZG8cD3fmK2ThC4FAMyvRb0qeobka3s22xTQ2kjwJO5gykTkZ+BR6SzRHQTjYMuT
iry9Bu8Kvbzu3r5n+/bmNz+xRNmEeehgT2qsHjA5b2YBVTr9MdN9Ro3H3saA3upr
oans248kpal88CGqsN2so/wZKxVnpiXlPHMdiNL7hRSUqlHkUi07FrP2Htg8kjI=
-----END CERTIFICATE-----`.trim();
    const c = new X509Certificate(certPem);
    assert(!c.ca);
    const signer = createSign('SHA256');
    assert(signer.sign(key, 'hex'));

    const c1 = new X509Certificate(certPem);
    assert(!c1.checkIssued(c1));
    const signer1 = createSign('SHA256');
    assert(signer1.sign(key, 'hex'));

    const c2 = new X509Certificate(certPem);
    assert(c2.toLegacyObject());
    const signer2 = createSign('SHA256');
    assert(signer2.sign(key, 'hex'));
  }

  // X509Certificate can be cloned via MessageChannel/MessagePort
  const mc = new MessageChannel();
  mc.port1.onmessage = common.mustCall(({ data }) => {
    assert(isX509Certificate(data));
    assert.deepStrictEqual(data.raw, x509.raw);
    mc.port1.close();
  });
  mc.port2.postMessage(x509);

  // Verify that legacy encoding works
  const legacyObjectCheck = {
    subject: 'C=US\n' +
      'ST=CA\n' +
      'L=SF\n' +
      'O=Joyent\n' +
      'OU=Node.js\n' +
      'CN=agent1\n' +
      'emailAddress=ry@tinyclouds.org',
    issuer:
      'C=US\n' +
      'ST=CA\n' +
      'L=SF\n' +
      'O=Joyent\n' +
      'OU=Node.js\n' +
      'CN=ca1\n' +
      'emailAddress=ry@tinyclouds.org',
    infoAccess:
      common.hasOpenSSL3 ?
        'OCSP - URI:http://ocsp.nodejs.org/\n' +
        'CA Issuers - URI:http://ca.nodejs.org/ca.cert' :
        'OCSP - URI:http://ocsp.nodejs.org/\n' +
        'CA Issuers - URI:http://ca.nodejs.org/ca.cert\n',
    modulus: 'EF5440701637E28ABB038E5641F828D834C342A9D25EDBB86A2BF' +
             '6FBD809CB8E037A98B71708E001242E4DEB54C6164885F599DD87' +
             'A23215745955BE20417E33C4D0D1B80C9DA3DE419A2607195D2FB' +
             '75657B0BBFB5EB7D0BBA5122D1B6964C7B570D50B8EC001EEB68D' +
             'FB584437508F3129928D673B30A3E0BF4F50609E6371',
    bits: 1024,
    exponent: '0x10001',
    valid_from: 'Nov 16 18:42:21 2018 GMT',
    valid_to: 'Aug 30 18:42:21 2292 GMT',
    fingerprint: 'D7:FD:F6:42:92:A8:83:51:8E:80:48:62:66:DA:85:C2:EE:A6:A1:CD',
    fingerprint256:
      'B0:BE:46:49:B8:29:63:E0:6F:63:C8:8A:57:9C:3F:9B:72:' +
      'C6:F5:89:E3:0D:84:AC:5B:08:9A:20:89:B6:8F:D6',
    fingerprint512:
      'D0:05:01:82:2C:D8:09:BE:27:94:E7:83:F1:88:BC:7A:8B:' +
      'D0:39:97:54:B6:D0:B4:46:5B:DE:13:5B:68:86:B6:F2:A8:' +
      '95:22:D5:6E:8B:35:DA:89:29:CA:A3:06:C5:CE:43:C1:7F:' +
      '2D:7E:5F:44:A5:EE:A3:CB:97:05:A3:E3:68',
    serialNumber: 'ECC9B856270DA9A8'
  };

  const legacyObject = x509.toLegacyObject();

  assert.deepStrictEqual(legacyObject.raw, x509.raw);
  assert.strictEqual(legacyObject.subject, legacyObjectCheck.subject);
  assert.strictEqual(legacyObject.issuer, legacyObjectCheck.issuer);
  assert.strictEqual(legacyObject.infoAccess, legacyObjectCheck.infoAccess);
  assert.strictEqual(legacyObject.modulus, legacyObjectCheck.modulus);
  assert.strictEqual(legacyObject.bits, legacyObjectCheck.bits);
  assert.strictEqual(legacyObject.exponent, legacyObjectCheck.exponent);
  assert.strictEqual(legacyObject.valid_from, legacyObjectCheck.valid_from);
  assert.strictEqual(legacyObject.valid_to, legacyObjectCheck.valid_to);
  assert.strictEqual(legacyObject.fingerprint, legacyObjectCheck.fingerprint);
  assert.strictEqual(
    legacyObject.fingerprint256,
    legacyObjectCheck.fingerprint256);
  assert.strictEqual(
    legacyObject.serialNumber,
    legacyObjectCheck.serialNumber);
}

{
  // This X.509 Certificate can be parsed by OpenSSL because it contains a
  // structurally sound TBSCertificate structure. However, the SPKI field of the
  // TBSCertificate contains the subjectPublicKey as a BIT STRING, and this bit
  // sequence is not a valid public key. Ensure that X509Certificate.publicKey
  // does not abort in this case.

  const certPem = `-----BEGIN CERTIFICATE-----
MIIDpDCCAw0CFEc1OZ8g17q+PZnna3iQ/gfoZ7f3MA0GCSqGSIb3DQEBBQUAMIHX
MRMwEQYLKwYBBAGCNzwCAQMTAkdJMR0wGwYDVQQPExRQcml2YXRlIE9yZ2FuaXph
dGlvbjEOMAwGA1UEBRMFOTkxOTExCzAJBgNVBAYTAkdJMRIwEAYDVQQIFAlHaWJy
YWx0YXIxEjAQBgNVBAcUCUdpYnJhbHRhcjEgMB4GA1UEChQXV0hHIChJbnRlcm5h
dGlvbmFsKSBMdGQxHDAaBgNVBAsUE0ludGVyYWN0aXZlIEJldHRpbmcxHDAaBgNV
BAMUE3d3dy53aWxsaWFtaGlsbC5jb20wIhgPMjAxNDAyMDcwMDAwMDBaGA8yMDE1
MDIyMTIzNTk1OVowgbAxCzAJBgNVBAYTAklUMQ0wCwYDVQQIEwRSb21lMRAwDgYD
VQQHEwdQb21lemlhMRYwFAYDVQQKEw1UZWxlY29taXRhbGlhMRIwEAYDVQQrEwlB
RE0uQVAuUE0xHTAbBgNVBAMTFHd3dy50ZWxlY29taXRhbGlhLml0MTUwMwYJKoZI
hvcNAQkBFiZ2YXNlc2VyY2l6aW9wb3J0YWxpY29AdGVsZWNvbWl0YWxpYS5pdDCB
nzANBgkqhkiG9w0BAQEFAAOBjQA4gYkCgYEA5m/Vf7PevH+inMfUJOc8GeR7WVhM
CQwcMM5k46MSZo7kCk7VZuaq5G2JHGAGnLPaPUkeXlrf5qLpTxXXxHNtz+WrDlFt
boAdnTcqpX3+72uBGOaT6Wi/9YRKuCs5D5/cAxAc3XjHfpRXMoXObj9Vy7mLndfV
/wsnTfU9QVeBkgsCAwEAAaOBkjCBjzAdBgNVHQ4EFgQUfLjAjEiC83A+NupGrx5+
Qe6nhRMwbgYIKwYBBQUHAQwEYjBgoV6gXDBaMFgwVhYJaW1hZ2UvZ2lmMCEwHzAH
BgUrDgMCGgQUS2u5KJYGDLvQUjibKaxLB4shBRgwJhYkaHR0cDovL2xvZ28udmVy
aXNpZ24uY29tL3ZzbG9nbzEuZ2lmMA0GCSqGSIb3DQEBBQUAA4GBALLiAMX0cIMp
+V/JgMRhMEUKbrt5lYKfv9dil/f22ezZaFafb070jGMMPVy9O3/PavDOkHtTv3vd
tAt3hIKFD1bJt6c6WtMH2Su3syosWxmdmGk5ihslB00lvLpfj/wed8i3bkcB1doq
UcXd/5qu2GhokrKU2cPttU+XAN2Om6a0
-----END CERTIFICATE-----`;

  const cert = new X509Certificate(certPem);
  assert.throws(() => cert.publicKey, {
    message: common.hasOpenSSL3 ? /decode error/ : /wrong tag/,
    name: 'Error'
  });

  assert.strictEqual(cert.checkIssued(cert), false);
}

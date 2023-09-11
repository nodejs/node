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
  '308203e8308202d0a0030201020214147d36c1c2f74206de9fab5f2226d78adb00a42630' +
  '0d06092a864886f70d01010b0500307a310b3009060355040613025553310b3009060355' +
  '04080c024341310b300906035504070c025346310f300d060355040a0c064a6f79656e74' +
  '3110300e060355040b0c074e6f64652e6a73310c300a06035504030c036361313120301e' +
  '06092a864886f70d010901161172794074696e79636c6f7564732e6f72673020170d3232' +
  '303930333231343033375a180f32323936303631373231343033375a307d310b30090603' +
  '55040613025553310b300906035504080c024341310b300906035504070c025346310f30' +
  '0d060355040a0c064a6f79656e743110300e060355040b0c074e6f64652e6a73310f300d' +
  '06035504030c066167656e74313120301e06092a864886f70d010901161172794074696e' +
  '79636c6f7564732e6f726730820122300d06092a864886f70d01010105000382010f0030' +
  '82010a0282010100d456320afb20d3827093dc2c4284ed04dfbabd56e1ddae529e28b790' +
  'cd4256db273349f3735ffd337c7a6363ecca5a27b7f73dc7089a96c6d886db0c62388f1c' +
  'dd6a963afcd599d5800e587a11f908960f84ed50ba25a28303ecda6e684fbe7baedc9ce8' +
  '801327b1697af25097cee3f175e400984c0db6a8eb87be03b4cf94774ba56fffc8c63c68' +
  'd6adeb60abbe69a7b14ab6a6b9e7baa89b5adab8eb07897c07f6d4fa3d660dff574107d2' +
  '8e8f63467a788624c574197693e959cea1362ffae1bba10c8c0d88840abfef103631b2e8' +
  'f5c39b5548a7ea57e8a39f89291813f45a76c448033a2b7ed8403f4baa147cf35e2d2554' +
  'aa65ce49695797095bf4dc6b0203010001a361305f305d06082b06010505070101045130' +
  '4f302306082b060105050730018617687474703a2f2f6f6373702e6e6f64656a732e6f72' +
  '672f302806082b06010505073002861c687474703a2f2f63612e6e6f64656a732e6f7267' +
  '2f63612e63657274300d06092a864886f70d01010b05000382010100c3349810632ccb7d' +
  'a585de3ed51e34ed154f0f7215608cf2701c00eda444dc2427072c8aca4da6472c1d9e68' +
  'f177f99a90a8b5dbf3884586d61cb1c14ea7016c8d38b70d1b46b42947db30edc1e9961e' +
  'd46c0f0e35da427bfbe52900771817e733b371adf19e12137235141a34347db0dfc05579' +
  '8b1f269f3bdf5e30ce35d1339d56bb3c570de9096215433047f87ca42447b44e7e6b5d0e' +
  '48f7894ab186f85b6b1a74561b520952fea888617f32f582afce1111581cd63efcc68986' +
  '00d248bb684dedb9c3d6710c38de9e9bc21f9c3394b729d5f707d64ea890603e5989f8fa' +
  '59c19ad1a00732e7adc851b89487cc00799dde068aa64b3b8fd976e8bc113ef2',
  'hex');

{
  const x509 = new X509Certificate(cert);

  assert(isX509Certificate(x509));

  assert(!x509.ca);
  assert.strictEqual(x509.subject, subjectCheck);
  assert.strictEqual(x509.subjectAltName, undefined);
  assert.strictEqual(x509.issuer, issuerCheck);
  assert.strictEqual(x509.infoAccess, infoAccessCheck);
  assert.strictEqual(x509.validFrom, 'Sep  3 21:40:37 2022 GMT');
  assert.strictEqual(x509.validTo, 'Jun 17 21:40:37 2296 GMT');
  assert.strictEqual(
    x509.fingerprint,
    '8B:89:16:C4:99:87:D2:13:1A:64:94:36:38:A5:32:01:F0:95:3B:53');
  assert.strictEqual(
    x509.fingerprint256,
    '2C:62:59:16:91:89:AB:90:6A:3E:98:88:A6:D3:C5:58:58:6C:AE:FF:9C:33:' +
    '22:7C:B6:77:D3:34:E7:53:4B:05'
  );
  assert.strictEqual(
    x509.fingerprint512,
    '0B:6F:D0:4D:6B:22:53:99:66:62:51:2D:2C:96:F2:58:3F:95:1C:CC:4C:44:' +
    '9D:B5:59:AA:AD:A8:F6:2A:24:8A:BB:06:A5:26:42:52:30:A3:37:61:30:A9:' +
    '5A:42:63:E0:21:2F:D6:70:63:07:96:6F:27:A7:78:12:08:02:7A:8B'
  );
  assert.strictEqual(x509.keyUsage, undefined);
  assert.strictEqual(x509.serialNumber, '147D36C1C2F74206DE9FAB5F2226D78ADB00A426');

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
    subject: Object.assign({ __proto__: null }, {
      C: 'US',
      ST: 'CA',
      L: 'SF',
      O: 'Joyent',
      OU: 'Node.js',
      CN: 'agent1',
      emailAddress: 'ry@tinyclouds.org',
    }),
    issuer: Object.assign({ __proto__: null }, {
      C: 'US',
      ST: 'CA',
      L: 'SF',
      O: 'Joyent',
      OU: 'Node.js',
      CN: 'ca1',
      emailAddress: 'ry@tinyclouds.org',
    }),
    infoAccess: Object.assign({ __proto__: null }, {
      'OCSP - URI': ['http://ocsp.nodejs.org/'],
      'CA Issuers - URI': ['http://ca.nodejs.org/ca.cert']
    }),
    modulus: 'D456320AFB20D3827093DC2C4284ED04DFBABD56E1DDAE529E28B790CD42' +
              '56DB273349F3735FFD337C7A6363ECCA5A27B7F73DC7089A96C6D886DB0C' +
              '62388F1CDD6A963AFCD599D5800E587A11F908960F84ED50BA25A28303EC' +
              'DA6E684FBE7BAEDC9CE8801327B1697AF25097CEE3F175E400984C0DB6A8' +
              'EB87BE03B4CF94774BA56FFFC8C63C68D6ADEB60ABBE69A7B14AB6A6B9E7' +
              'BAA89B5ADAB8EB07897C07F6D4FA3D660DFF574107D28E8F63467A788624' +
              'C574197693E959CEA1362FFAE1BBA10C8C0D88840ABFEF103631B2E8F5C3' +
              '9B5548A7EA57E8A39F89291813F45A76C448033A2B7ED8403F4BAA147CF3' +
              '5E2D2554AA65CE49695797095BF4DC6B',
    bits: 2048,
    exponent: '0x10001',
    valid_from: 'Sep  3 21:40:37 2022 GMT',
    valid_to: 'Jun 17 21:40:37 2296 GMT',
    fingerprint: '8B:89:16:C4:99:87:D2:13:1A:64:94:36:38:A5:32:01:F0:95:3B:53',
    fingerprint256:
      '2C:62:59:16:91:89:AB:90:6A:3E:98:88:A6:D3:C5:58:58:6C:AE:FF:9C:33:' +
      '22:7C:B6:77:D3:34:E7:53:4B:05',
    fingerprint512:
      '51:62:18:39:E2:E2:77:F5:86:11:E8:C0:CA:54:43:7C:76:83:19:05:D0:03:' +
      '24:21:B8:EB:14:61:FB:24:16:EB:BD:51:1A:17:91:04:30:03:EB:68:5F:DC:' +
      '86:E1:D1:7C:FB:AF:78:ED:63:5F:29:9C:32:AF:A1:8E:22:96:D1:02',
    serialNumber: '147D36C1C2F74206DE9FAB5F2226D78ADB00A426'
  };

  const legacyObject = x509.toLegacyObject();

  assert.deepStrictEqual(legacyObject.raw, x509.raw);
  assert.deepStrictEqual(legacyObject.subject, legacyObjectCheck.subject);
  assert.deepStrictEqual(legacyObject.issuer, legacyObjectCheck.issuer);
  assert.deepStrictEqual(legacyObject.infoAccess, legacyObjectCheck.infoAccess);
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

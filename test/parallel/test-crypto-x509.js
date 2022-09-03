// Flags: --expose-internals
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const {
  X509Certificate,
  createPrivateKey,
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
  '3082035c308202c5a003020102020900ecc9b856270da9aa300d06092a864886f70d0101' +
  '0b0500307a310b3009060355040613025553310b300906035504080c024341310b300906' +
  '035504070c025346310f300d060355040a0c064a6f79656e743110300e060355040b0c07' +
  '4e6f64652e6a73310c300a06035504030c036361313120301e06092a864886f70d010901' +
  '161172794074696e79636c6f7564732e6f72673020170d3232303930333134343635315a' +
  '180f32323936303631373134343635315a307d310b3009060355040613025553310b3009' +
  '06035504080c024341310b300906035504070c025346310f300d060355040a0c064a6f79' +
  '656e743110300e060355040b0c074e6f64652e6a73310f300d06035504030c066167656e' +
  '74313120301e06092a864886f70d010901161172794074696e79636c6f7564732e6f7267' +
  '30820122300d06092a864886f70d01010105000382010f003082010a0282010100d45632' +
  '0afb20d3827093dc2c4284ed04dfbabd56e1ddae529e28b790cd4256db273349f3735ffd' +
  '337c7a6363ecca5a27b7f73dc7089a96c6d886db0c62388f1cdd6a963afcd599d5800e58' +
  '7a11f908960f84ed50ba25a28303ecda6e684fbe7baedc9ce8801327b1697af25097cee3' +
  'f175e400984c0db6a8eb87be03b4cf94774ba56fffc8c63c68d6adeb60abbe69a7b14ab6' +
  'a6b9e7baa89b5adab8eb07897c07f6d4fa3d660dff574107d28e8f63467a788624c57419' +
  '7693e959cea1362ffae1bba10c8c0d88840abfef103631b2e8f5c39b5548a7ea57e8a39f' +
  '89291813f45a76c448033a2b7ed8403f4baa147cf35e2d2554aa65ce49695797095bf4dc' +
  '6b0203010001a361305f305d06082b060105050701010451304f302306082b0601050507' +
  '30018617687474703a2f2f6f6373702e6e6f64656a732e6f72672f302806082b06010505' +
  '073002861c687474703a2f2f63612e6e6f64656a732e6f72672f63612e63657274300d06' +
  '092a864886f70d01010b0500038181001f3ab6ce83b0af3a78a5de6707480de7e6a516d9' +
  '207b9fd5facb77c7eed3d98458a186cd2ffebc91640ca124cf1a59a9b5e08e60bbda0f0b' +
  '7458da6fc8dbc5b17a23436eba314c7aada265f70f5ab88f1902012dd2bfa8a92a5c0020' +
  '914f5c83c6cda906381e2844ff37c20dcc720e88d3d01f1bf63aeb4b7427c047e7b44603',
  'hex');

{
  const x509 = new X509Certificate(cert);

  assert(isX509Certificate(x509));

  assert(!x509.ca);
  assert.strictEqual(x509.subject, subjectCheck);
  assert.strictEqual(x509.subjectAltName, undefined);
  assert.strictEqual(x509.issuer, issuerCheck);
  assert.strictEqual(x509.infoAccess, infoAccessCheck);
  assert.strictEqual(x509.validFrom, 'Sep  3 14:46:51 2022 GMT');
  assert.strictEqual(x509.validTo, 'Jun 17 14:46:51 2296 GMT');
  assert.strictEqual(
    x509.fingerprint,
    '39:3C:63:64:25:25:9B:BC:5B:51:6D:05:EE:DA:6F:40:4A:E5:54:06');
  assert.strictEqual(
    x509.fingerprint256,
    '05:C8:51:4C:42:C9:E7:6E:4D:78:BE:9B:48:F6:B6:C8:A0:97:7F:42:87:B5:' +
    '06:97:E7:DE:A5:3A:4D:BE:BA:CC'
  );
  assert.strictEqual(
    x509.fingerprint512,
    '51:62:18:39:E2:E2:77:F5:86:11:E8:C0:CA:54:43:7C:76:83:19:05:D0:03:' +
    '24:21:B8:EB:14:61:FB:24:16:EB:BD:51:1A:17:91:04:30:03:EB:68:5F:DC:' +
    '86:E1:D1:7C:FB:AF:78:ED:63:5F:29:9C:32:AF:A1:8E:22:96:D1:02'
  );
  assert.strictEqual(x509.keyUsage, undefined);
  assert.strictEqual(x509.serialNumber, 'ECC9B856270DA9AA');

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
    subject: Object.assign(Object.create(null), {
      C: 'US',
      ST: 'CA',
      L: 'SF',
      O: 'Joyent',
      OU: 'Node.js',
      CN: 'agent1',
      emailAddress: 'ry@tinyclouds.org',
    }),
    issuer: Object.assign(Object.create(null), {
      C: 'US',
      ST: 'CA',
      L: 'SF',
      O: 'Joyent',
      OU: 'Node.js',
      CN: 'ca1',
      emailAddress: 'ry@tinyclouds.org',
    }),
    infoAccess: Object.assign(Object.create(null), {
      'OCSP - URI': ['http://ocsp.nodejs.org/'],
      'CA Issuers - URI': ['http://ca.nodejs.org/ca.cert']
    }),
    modulus:  'D456320AFB20D3827093DC2C4284ED04DFBABD56E1DDAE529E28B790CD42' +
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
    valid_from: 'Sep  3 14:46:51 2022 GMT',
    valid_to: 'Jun 17 14:46:51 2296 GMT',
    fingerprint: '39:3C:63:64:25:25:9B:BC:5B:51:6D:05:EE:DA:6F:40:4A:E5:54:06',
    fingerprint256:
      '05:C8:51:4C:42:C9:E7:6E:4D:78:BE:9B:48:F6:B6:C8:A0:' +
      '97:7F:42:87:B5:06:97:E7:DE:A5:3A:4D:BE:BA:CC',
    fingerprint512:
      '51:62:18:39:E2:E2:77:F5:86:11:E8:C0:CA:54:43:7C:76:83:19:05:D0:03:' +
      '24:21:B8:EB:14:61:FB:24:16:EB:BD:51:1A:17:91:04:30:03:EB:68:5F:DC:' +
      '86:E1:D1:7C:FB:AF:78:ED:63:5F:29:9C:32:AF:A1:8E:22:96:D1:02',
    serialNumber: 'ECC9B856270DA9AA'
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

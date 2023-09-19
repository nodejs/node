// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const crypto = require('crypto');

// Verify that detailed getPeerCertificate() return value has all certs.

const {
  assert, connect, debug, keys
} = require(fixtures.path('tls-connect'));

function sha256(s) {
  return crypto.createHash('sha256').update(s);
}

connect({
  client: { rejectUnauthorized: false },
  server: keys.agent1,
}, function(err, pair, cleanup) {
  assert.ifError(err);
  const socket = pair.client.conn;
  const localCert = socket.getCertificate();
  assert.deepStrictEqual(localCert, {});
  let peerCert = socket.getPeerCertificate();
  assert.ok(!peerCert.issuerCertificate);

  peerCert = socket.getPeerCertificate(true);
  debug('peerCert:\n', peerCert);

  assert.ok(peerCert.issuerCertificate);
  assert.strictEqual(peerCert.ca, false);
  assert.strictEqual(peerCert.issuerCertificate.ca, true);
  assert.strictEqual(peerCert.subject.emailAddress, 'ry@tinyclouds.org');
  assert.strictEqual(peerCert.serialNumber, '147D36C1C2F74206DE9FAB5F2226D78ADB00A426');
  assert.strictEqual(peerCert.exponent, '0x10001');
  assert.strictEqual(peerCert.bits, 2048);
  // The conversion to bits is odd because modulus isn't a buffer, its a hex
  // string. There are two hex chars for every byte of modulus, and 8 bits per
  // byte.
  assert.strictEqual(peerCert.modulus.length / 2 * 8, peerCert.bits);
  assert.strictEqual(
    peerCert.fingerprint,
    '8B:89:16:C4:99:87:D2:13:1A:64:94:36:38:A5:32:01:F0:95:3B:53'
  );
  assert.strictEqual(
    peerCert.fingerprint256,
    '2C:62:59:16:91:89:AB:90:6A:3E:98:88:A6:D3:C5:58:58:6C:AE:FF:9C:33:' +
    '22:7C:B6:77:D3:34:E7:53:4B:05',
  );
  assert.strictEqual(
    peerCert.fingerprint512,
    '0B:6F:D0:4D:6B:22:53:99:66:62:51:2D:2C:96:F2:58:3F:95:1C:CC:4C:44:' +
    '9D:B5:59:AA:AD:A8:F6:2A:24:8A:BB:06:A5:26:42:52:30:A3:37:61:30:A9:' +
    '5A:42:63:E0:21:2F:D6:70:63:07:96:6F:27:A7:78:12:08:02:7A:8B'
  );

  // SHA256 fingerprint of the public key
  assert.strictEqual(
    sha256(peerCert.pubkey).digest('hex'),
    '490f8da0889339df5164d500b406de0af3249c174c2b60152528940fa116e9cc'
  );

  // HPKP / RFC7469 "pin-sha256" of the public key
  assert.strictEqual(
    sha256(peerCert.pubkey).digest('base64'),
    'SQ+NoIiTOd9RZNUAtAbeCvMknBdMK2AVJSiUD6EW6cw='
  );

  assert.deepStrictEqual(peerCert.infoAccess['OCSP - URI'],
                         [ 'http://ocsp.nodejs.org/' ]);

  const issuer = peerCert.issuerCertificate;
  assert.strictEqual(issuer.issuerCertificate, issuer);
  assert.strictEqual(issuer.serialNumber, '4AB16C8DFD6A7D0D2DFCABDF9C4B0E92C6AD0229');

  return cleanup();
});

connect({
  client: { rejectUnauthorized: false },
  server: keys.ec,
}, function(err, pair, cleanup) {
  assert.ifError(err);
  const socket = pair.client.conn;
  let peerCert = socket.getPeerCertificate(true);
  assert.ok(peerCert.issuerCertificate);

  peerCert = socket.getPeerCertificate(true);
  debug('peerCert:\n', peerCert);

  assert.ok(peerCert.issuerCertificate);
  assert.strictEqual(peerCert.subject.emailAddress, 'ry@tinyclouds.org');
  assert.strictEqual(peerCert.serialNumber, '32E8197681DA33185867B52885F678BFDBA51727');
  assert.strictEqual(peerCert.exponent, undefined);
  assert.strictEqual(peerCert.pubKey, undefined);
  assert.strictEqual(peerCert.modulus, undefined);
  assert.strictEqual(
    peerCert.fingerprint,
    '31:EB:2C:7B:AA:39:E8:E8:F5:43:62:05:CD:64:B3:66:1E:EA:44:A3'
  );
  assert.strictEqual(
    peerCert.fingerprint256,
    'B9:27:E4:8F:C0:F5:E3:FD:A6:E5:96:11:DB:69:B8:80:94:8B:0F:6A:4C:D6:80:4F:' +
    '87:31:3C:A3:77:6C:4C:0A'
  );
  assert.strictEqual(
    peerCert.fingerprint512,
    '45:E3:ED:6E:22:1C:3C:DD:D7:E1:65:A9:30:6E:79:0C:9F:98:B8:BC:24:BB:BA:32:' +
    '54:4D:70:4E:78:4F:1B:97:3C:A7:F5:DB:06:F1:36:E9:53:4C:0A:D2:86:83:79:8A:' +
    '72:2B:81:55:5D:6F:BC:A6:5B:61:85:26:6B:9D:3E:E8'
  );

  assert.strictEqual(
    sha256(peerCert.pubkey).digest('hex'),
    'ec68fc7d5e32cd4e1da5a7b59c0a2229be6f82fcc9bf8c8691a2262aacb14f53'
  );
  assert.strictEqual(peerCert.asn1Curve, 'prime256v1');
  assert.strictEqual(peerCert.nistCurve, 'P-256');
  assert.strictEqual(peerCert.bits, 256);

  assert.strictEqual(peerCert.infoAccess, undefined);

  const issuer = peerCert.issuerCertificate;
  assert.strictEqual(issuer.issuerCertificate, issuer);
  assert.strictEqual(issuer.serialNumber, '32E8197681DA33185867B52885F678BFDBA51727');

  return cleanup();
});

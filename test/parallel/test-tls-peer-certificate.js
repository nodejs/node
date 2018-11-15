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
  assert.strictEqual(peerCert.subject.emailAddress, 'ry@tinyclouds.org');
  assert.strictEqual(peerCert.serialNumber, 'ECC9B856270DA9A8');
  assert.strictEqual(peerCert.exponent, '0x10001');
  assert.strictEqual(peerCert.bits, 1024);
  // The conversion to bits is odd because modulus isn't a buffer, its a hex
  // string. There are two hex chars for every byte of modulus, and 8 bits per
  // byte.
  assert.strictEqual(peerCert.modulus.length / 2 * 8, peerCert.bits);
  assert.strictEqual(
    peerCert.fingerprint,
    'D7:FD:F6:42:92:A8:83:51:8E:80:48:62:66:DA:85:C2:EE:A6:A1:CD'
  );
  assert.strictEqual(
    peerCert.fingerprint256,
    'B0:BE:46:49:B8:29:63:E0:6F:63:C8:8A:57:9C:3F:9B:72:C6:F5:89:E3:0D:84:AC:' +
    '5B:08:9A:20:89:B6:8F:D6'
  );

  // SHA256 fingerprint of the public key
  assert.strictEqual(
    sha256(peerCert.pubkey).digest('hex'),
    '221fcc8593146e9eee65b2f7f9c1504993ece8de014657a4a1cde55c5e35d06e'
  );

  // HPKP / RFC7469 "pin-sha256" of the public key
  assert.strictEqual(
    sha256(peerCert.pubkey).digest('base64'),
    'Ih/MhZMUbp7uZbL3+cFQSZPs6N4BRlekoc3lXF410G4='
  );

  assert.deepStrictEqual(peerCert.infoAccess['OCSP - URI'],
                         [ 'http://ocsp.nodejs.org/' ]);

  const issuer = peerCert.issuerCertificate;
  assert.strictEqual(issuer.issuerCertificate, issuer);
  assert.strictEqual(issuer.serialNumber, 'CB153AE212609FC6');

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
  assert.strictEqual(peerCert.serialNumber, 'C1EA7B03D5956D52');
  assert.strictEqual(peerCert.exponent, undefined);
  assert.strictEqual(peerCert.pubKey, undefined);
  assert.strictEqual(peerCert.modulus, undefined);
  assert.strictEqual(
    peerCert.fingerprint,
    'DF:F0:D3:6B:C3:E7:74:7C:C7:F3:FB:1E:33:12:AE:6C:8D:53:5F:74'
  );
  assert.strictEqual(
    peerCert.fingerprint256,
    'AB:08:3C:40:C7:07:D7:D1:79:32:92:3B:96:52:D0:38:4C:22:ED:CD:23:51:D0:A1:' +
    '67:AA:33:A0:D5:26:5C:41'
  );

  assert.strictEqual(
    sha256(peerCert.pubkey).digest('hex'),
    'ec68fc7d5e32cd4e1da5a7b59c0a2229be6f82fcc9bf8c8691a2262aacb14f53'
  );
  assert.strictEqual(peerCert.asn1Curve, 'prime256v1');
  assert.strictEqual(peerCert.nistCurve, 'P-256');
  assert.strictEqual(peerCert.bits, 256);

  assert.deepStrictEqual(peerCert.infoAccess, undefined);

  const issuer = peerCert.issuerCertificate;
  assert.strictEqual(issuer.issuerCertificate, issuer);
  assert.strictEqual(issuer.serialNumber, 'C1EA7B03D5956D52');

  return cleanup();
});

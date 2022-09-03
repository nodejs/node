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
  assert.strictEqual(peerCert.serialNumber, 'ECC9B856270DA9AA');
  assert.strictEqual(peerCert.exponent, '0x10001');
  assert.strictEqual(peerCert.bits, 2048);
  // The conversion to bits is odd because modulus isn't a buffer, its a hex
  // string. There are two hex chars for every byte of modulus, and 8 bits per
  // byte.
  assert.strictEqual(peerCert.modulus.length / 2 * 8, peerCert.bits);
  assert.strictEqual(
    peerCert.fingerprint,
    '39:3C:63:64:25:25:9B:BC:5B:51:6D:05:EE:DA:6F:40:4A:E5:54:06'
  );
  assert.strictEqual(
    peerCert.fingerprint256,
    '05:C8:51:4C:42:C9:E7:6E:4D:78:BE:9B:48:F6:B6:C8:A0:' +
    '97:7F:42:87:B5:06:97:E7:DE:A5:3A:4D:BE:BA:CC',
   );
  assert.strictEqual(
    peerCert.fingerprint512,
    '51:62:18:39:E2:E2:77:F5:86:11:E8:C0:CA:54:43:7C:76:83:19:05:D0:03:' +
    '24:21:B8:EB:14:61:FB:24:16:EB:BD:51:1A:17:91:04:30:03:EB:68:5F:DC:' +
    '86:E1:D1:7C:FB:AF:78:ED:63:5F:29:9C:32:AF:A1:8E:22:96:D1:02'
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
    peerCert.fingerprint512,
    '52:F4:86:64:69:0F:94:6F:78:DE:17:7A:5C:92:CA:0F:05:2E:6A:EE:4A:FF:8B:39:' +
    'D6:5E:7A:B8:08:77:B3:80:FC:40:A1:F4:58:CC:7A:DF:BB:3D:38:F9:02:77:50:2B:' +
    '9A:0B:FB:79:B4:BD:E9:F5:9C:44:C8:5D:D5:F0:E2:BC'
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
  assert.strictEqual(issuer.serialNumber, 'C1EA7B03D5956D52');

  return cleanup();
});

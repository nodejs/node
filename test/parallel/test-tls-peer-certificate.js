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
  let peerCert = socket.getPeerCertificate();
  assert.ok(!peerCert.issuerCertificate);

  peerCert = socket.getPeerCertificate(true);
  debug('peerCert:\n', peerCert);

  assert.ok(peerCert.issuerCertificate);
  assert.strictEqual(peerCert.subject.emailAddress, 'ry@tinyclouds.org');
  assert.strictEqual(peerCert.serialNumber, '85C7EBF3D34DC356');
  assert.strictEqual(peerCert.exponent, '0x10001');
  assert.strictEqual(
    peerCert.fingerprint,
    '98:AC:1D:27:A8:50:60:5B:4B:79:F5:02:C2:84:1D:E0:4C:41:CD:36'
  );
  assert.strictEqual(
    peerCert.fingerprint256,
    '45:31:DB:0F:7B:0C:7D:CB:5D:1E:BC:57:8A:BA:29:99:5B:D5:BC:12:73:D3:F6:33:' +
    '28:77:7A:E2:3F:55:60:AD'
  );

  // SHA256 fingerprint of the public key
  assert.strictEqual(
    sha256(peerCert.pubkey).digest('hex'),
    '957b44fc84e34c9c262f6ffed31b470395f96e24c16aefc5cfe4110d621136df'
  );

  // HPKP / RFC7469 "pin-sha256" of the public key
  assert.strictEqual(
    sha256(peerCert.pubkey).digest('base64'),
    'lXtE/ITjTJwmL2/+0xtHA5X5biTBau/Fz+QRDWIRNt8='
  );

  assert.deepStrictEqual(peerCert.infoAccess['OCSP - URI'],
                         [ 'http://ocsp.nodejs.org/' ]);

  const issuer = peerCert.issuerCertificate;
  assert.strictEqual(issuer.issuerCertificate, issuer);
  assert.strictEqual(issuer.serialNumber, '80530D47F2900BD5');

  return cleanup();
});

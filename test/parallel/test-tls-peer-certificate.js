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
  assert, connect, debug, keys,
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
  assert.strictEqual(peerCert.serialNumber, '9A84ABCFB8A72AC0');
  assert.strictEqual(peerCert.exponent, '0x10001');
  assert.strictEqual(
    peerCert.fingerprint,
    '8D:06:3A:B3:E5:8B:85:29:72:4F:7D:1B:54:CD:95:19:3C:EF:6F:AA'
  );
  assert.strictEqual(
    peerCert.fingerprint256,
    'A1:DC:01:1A:EC:A3:7B:86:A8:C2:3E:26:9F:EB:EE:5C:A9:3B:BE:06' +
    ':4C:A4:00:53:93:A9:66:07:A7:BC:13:32'
  );

  // SHA256 fingerprint of the public key
  assert.strictEqual(
    sha256(peerCert.pubkey).digest('hex'),
    'fa5152e4407bad1e7537ef5bfc3f19fa9a62ee04432fd75e109b1803704c31ba'
  );

  // HPKP / RFC7469 "pin-sha256" of the public key
  assert.strictEqual(
    sha256(peerCert.pubkey).digest('base64'),
    '+lFS5EB7rR51N+9b/D8Z+ppi7gRDL9deEJsYA3BMMbo='
  );

  assert.deepStrictEqual(peerCert.infoAccess['OCSP - URI'],
                         [ 'http://ocsp.nodejs.org/' ]);

  const issuer = peerCert.issuerCertificate;
  assert.strictEqual(issuer.issuerCertificate, issuer);
  assert.strictEqual(issuer.serialNumber, '8DF21C01468AF393');

  return cleanup();
});

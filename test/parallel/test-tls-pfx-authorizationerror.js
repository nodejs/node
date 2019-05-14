'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('node compiled without crypto.');
const fixtures = require('../common/fixtures');

// This test ensures that TLS does not fail to read a self-signed certificate
// and thus throw an `authorizationError`.
// https://github.com/nodejs/node/issues/5100

const assert = require('assert');
const tls = require('tls');

const pfx = fixtures.readKey('agent1.pfx');

const server = tls
  .createServer(
    {
      pfx: pfx,
      passphrase: 'sample',
      requestCert: true,
      rejectUnauthorized: false
    },
    common.mustCall(function(c) {
      assert.strictEqual(c.getPeerCertificate().serialNumber,
                         'ECC9B856270DA9A8');
      assert.strictEqual(c.authorizationError, null);
      c.end();
    })
  )
  .listen(0, function() {
    const client = tls.connect(
      {
        port: this.address().port,
        pfx: pfx,
        passphrase: 'sample',
        rejectUnauthorized: false
      },
      function() {
        for (let i = 0; i < 10; ++i) {
          // Calling this repeatedly is a regression test that verifies
          // that .getCertificate() does not accidentally decrease the
          // reference count of the X509* certificate on the native side.
          assert.strictEqual(client.getCertificate().serialNumber,
                             'ECC9B856270DA9A8');
        }
        client.end();
        server.close();
      }
    );
  });

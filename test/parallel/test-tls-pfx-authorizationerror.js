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

const pfx = fixtures.readKey('agent1-pfx.pem');

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
                         'FAD50CC6A07F516C');
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
        assert.strictEqual(client.getCertificate().serialNumber,
                           'FAD50CC6A07F516C');
        client.end();
        server.close();
      }
    );
  });

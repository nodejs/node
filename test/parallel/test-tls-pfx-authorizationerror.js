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
const { hasFIPS } = require('../common/crypto');

const fips3 = hasFIPS(3);
const fips35 = hasFIPS(3, 5);
const fips4 = hasFIPS(4);
const pfx = fixtures.readKey(fips35 ? 'agent1-fips.pfx' : 'agent1.pfx');
const passphrase = fips35 ? 'password' : 'sample';

if (fips3) {
  assert.throws(() => tls.createServer({
    pfx: fixtures.readKey('agent1.pfx'),
    passphrase: 'sample',
  }), {
    code: 'ERR_CRYPTO_UNSUPPORTED_OPERATION',
  });

  if (!fips35) {
    return;
  }

  if (fips4) {
    assert.throws(() => tls.createServer({ pfx, passphrase: 'sample' }), {
      message: 'password strength too weak',
    });
  }
}

const server = tls
  .createServer(
    {
      pfx: pfx,
      passphrase,
      requestCert: true,
      rejectUnauthorized: false
    },
    common.mustCall(function(c) {
      assert.strictEqual(c.getPeerCertificate().serialNumber,
                         '147D36C1C2F74206DE9FAB5F2226D78ADB00A426');
      assert.strictEqual(c.authorizationError, null);
      c.end();
    })
  )
  .listen(0, common.mustCall(function() {
    const client = tls.connect(
      {
        port: this.address().port,
        pfx: pfx,
        passphrase,
        rejectUnauthorized: false
      },
      common.mustCall(() => {
        for (let i = 0; i < 10; ++i) {
          // Calling this repeatedly is a regression test that verifies
          // that .getCertificate() does not accidentally decrease the
          // reference count of the X509* certificate on the native side.
          assert.strictEqual(client.getCertificate().serialNumber,
                             '147D36C1C2F74206DE9FAB5F2226D78ADB00A426');
        }
        client.end();
        server.close();
      }),
    );
  }));

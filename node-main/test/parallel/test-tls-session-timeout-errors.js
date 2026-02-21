'use strict';
// This tests validation of sessionTimeout option in TLS server.
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('rsa_private.pem');
const cert = fixtures.readKey('rsa_cert.crt');

// Node.js should not allow setting negative timeouts since new versions of
// OpenSSL do not handle those as users might expect

for (const sessionTimeout of [-1, -100, -(2 ** 31)]) {
  assert.throws(() => {
    tls.createServer({
      key: key,
      cert: cert,
      ca: [cert],
      sessionTimeout,
      maxVersion: 'TLSv1.2',
    });
  }, {
    code: 'ERR_OUT_OF_RANGE',
    message: 'The value of "options.sessionTimeout" is out of range. It ' +
              `must be >= 0 && <= ${2 ** 31 - 1}. Received ${sessionTimeout}`,
  });
}

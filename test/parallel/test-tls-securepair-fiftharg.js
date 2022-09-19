'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const sslcontext = tls.createSecureContext({
  cert: fixtures.readKey('rsa_cert.crt'),
  key: fixtures.readKey('rsa_private.pem')
});

const pair = tls.createSecurePair(sslcontext, true, false, false, {
  SNICallback: common.mustCall((servername, cb) => {
    assert.strictEqual(servername, 'www.google.com');
  })
});

// Captured traffic from browser's request to https://www.google.com
const sslHello = fixtures.readSync('google_ssl_hello.bin');

pair.encrypted.write(sslHello);

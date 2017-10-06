'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const assert = require('assert');
const tls = require('tls');

// https://github.com/joyent/node/issues/1218
// uncatchable exception on TLS connection error
{
  const cert = fixtures.readSync('test_cert.pem');
  const key = fixtures.readSync('test_key.pem');

  const options = {cert: cert, key: key, port: common.PORT};
  const conn = tls.connect(options, common.fail);

  conn.on(
    'error',
    common.mustCall((e) => { assert.strictEqual(e.code, 'ECONNREFUSED'); })
  );
}

// SSL_accept/SSL_connect error handling
{
  const cert = fixtures.readSync('test_cert.pem');
  const key = fixtures.readSync('test_key.pem');

  const conn = tls.connect({
    cert: cert,
    key: key,
    port: common.PORT,
    ciphers: 'rick-128-roll'
  }, function() {
    assert.ok(false); // callback should never be executed
  });

  conn.on(
    'error',
    common.mustCall((e) => { assert.strictEqual(e.code, 'ECONNREFUSED'); })
  );
}

'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { hasOpenSSL } = require('../common/crypto');

{
  const options = {
    key: fixtures.readKey('agent2-key.pem'),
    cert: fixtures.readKey('agent2-cert.pem'),
    ciphers: 'DES-CBC-SHA'
  };
  assert.throws(() => tls.createServer(options, common.mustNotCall()),
                /no[_ ]cipher[_ ]match/i);
  options.ciphers = 'FOOBARBAZ';
  assert.throws(() => tls.createServer(options, common.mustNotCall()),
                /no[_ ]cipher[_ ]match/i);
  options.ciphers = 'TLS_not_a_cipher';
  assert.throws(() => tls.createServer(options, common.mustNotCall()),
                /no[_ ]cipher[_ ]match/i);
}

// Cipher name matching is case-sensitive prior to OpenSSL 4.0, and
// case-insensitive starting with OpenSSL 4.0.
{
  const options = {
    key: fixtures.readKey('agent2-key.pem'),
    cert: fixtures.readKey('agent2-cert.pem'),
    ciphers: 'aes256-sha',
  };
  if (hasOpenSSL(4, 0)) {
    tls.createServer(options).close();
  } else {
    assert.throws(() => tls.createServer(options, common.mustNotCall()),
                  /no[_ ]cipher[_ ]match/i);
  }
}

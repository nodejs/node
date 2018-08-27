'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

{
  const options = {
    key: fixtures.readKey('agent2-key.pem'),
    cert: fixtures.readKey('agent2-cert.pem'),
    ciphers: 'aes256-sha'
  };
  assert.throws(() => tls.createServer(options, common.mustNotCall()),
                /no cipher match/i);
  options.ciphers = 'FOOBARBAZ';
  assert.throws(() => tls.createServer(options, common.mustNotCall()),
                /no cipher match/i);
}

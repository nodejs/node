'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('node compiled without crypto.');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const pfx = fixtures.readKey('agent1-pfx.pem');

const server = tls.createServer({
  pfx: pfx,
  passphrase: 'sample',
  requestCert: true,
  rejectUnauthorized: false
}, common.mustCall(function(c) {
  assert.strictEqual(c.authorizationError, null);
  c.end();
})).listen(0, function() {
  const client = tls.connect({
    port: this.address().port,
    pfx: pfx,
    passphrase: 'sample',
    rejectUnauthorized: false
  }, function() {
    client.end();
    server.close();
  });
});

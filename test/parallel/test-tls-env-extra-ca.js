// Certs in NODE_EXTRA_CA_CERTS are used for TLS peer validation

'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const { fork } = require('child_process');

if (process.env.CHILD) {
  const copts = {
    port: process.env.PORT,
    checkServerIdentity: common.mustCall(),
  };
  const client = tls.connect(copts, common.mustCall(function() {
    client.end('hi');
  }));
  return;
}

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem'),
};

const server = tls.createServer(options, common.mustCall(function(s) {
  s.end('bye');
  server.close();
})).listen(0, common.mustCall(function() {
  const env = Object.assign({}, process.env, {
    CHILD: 'yes',
    PORT: this.address().port,
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'ca1-cert.pem')
  });

  fork(__filename, { env }).on('exit', common.mustCall(function(status) {
    assert.strictEqual(status, 0, 'client did not succeed in connecting');
  }));
}));

// Certs in NODE_EXTRA_CA_CERTS are used for TLS peer validation

'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const fs = require('fs');
const tls = require('tls');

const fork = require('child_process').fork;

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
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`),
};

const server = tls.createServer(options, common.mustCall(function(s) {
  s.end('bye');
  server.close();
})).listen(0, common.mustCall(function() {
  const env = {
    CHILD: 'yes',
    PORT: this.address().port,
    NODE_EXTRA_CA_CERTS: `${common.fixturesDir}/keys/ca1-cert.pem`,
  };

  fork(__filename, {env: env}).on('exit', common.mustCall(function(status) {
    assert.strictEqual(status, 0, 'client did not succeed in connecting');
  }));
}));

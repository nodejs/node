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
    pfx: fixtures.readKey('agent1.pfx'),
    passphrase: 'sample'
  };

  const client = tls.connect(copts, common.mustCall(() => {
    client.end('hi');
  }));

  return;
}

const options = {
  key: fixtures.readKey('agent3-key.pem'),
  cert: fixtures.readKey('agent3-cert.pem')
};

const server = tls.createServer(options, common.mustCall((socket) => {
  socket.end('bye');
  server.close();
})).listen(0, common.mustCall(() => {
  const env = Object.assign({}, process.env, {
    CHILD: 'yes',
    PORT: server.address().port,
    NODE_EXTRA_CA_CERTS: fixtures.path('keys', 'ca2-cert.pem')
  });

  fork(__filename, { env }).on('exit', common.mustCall((status) => {
    // Client did not succeed in connecting
    assert.strictEqual(status, 0);
  }));
}));

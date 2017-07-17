'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

common.refreshTmpDir();

const server = tls.Server(options, common.mustCall(function(socket) {
  server.close();
}));
server.listen(common.PIPE, common.mustCall(function() {
  const options = { rejectUnauthorized: false };
  const client = tls.connect(common.PIPE, options, common.mustCall(function() {
    client.end();
  }));
}));

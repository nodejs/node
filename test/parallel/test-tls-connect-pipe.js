'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
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

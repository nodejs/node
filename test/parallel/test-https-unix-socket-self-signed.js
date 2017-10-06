'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

common.refreshTmpDir();

const fixtures = require('../common/fixtures');
const https = require('https');
const options = {
  cert: fixtures.readSync('test_cert.pem'),
  key: fixtures.readSync('test_key.pem')
};

const server = https.createServer(options, common.mustCall((req, res) => {
  res.end('bye\n');
  server.close();
}));

server.listen(common.PIPE, common.mustCall(() => {
  https.get({
    socketPath: common.PIPE,
    rejectUnauthorized: false
  });
}));

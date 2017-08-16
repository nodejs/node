'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

common.refreshTmpDir();

const fs = require('fs');
const https = require('https');
const options = {
  cert: fs.readFileSync(`${common.fixturesDir}/test_cert.pem`),
  key: fs.readFileSync(`${common.fixturesDir}/test_key.pem`)
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

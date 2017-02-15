'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

// a server that never replies
const server = https.createServer(options, function() {
  console.log('Got request.  Doing nothing.');
}).listen(0, common.mustCall(function() {
  const req = https.request({
    host: 'localhost',
    port: this.address().port,
    path: '/',
    method: 'GET',
    rejectUnauthorized: false
  });
  req.setTimeout(10);
  req.end();

  req.on('response', function() {
    console.log('got response');
  });

  req.on('timeout', common.mustCall(function() {
    console.log('timeout occurred outside');
    req.destroy();
    server.close();
    process.exit(0);
  }));
}));

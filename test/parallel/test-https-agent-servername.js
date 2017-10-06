'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const https = require('https');
const fs = require('fs');

const options = {
  key: fs.readFileSync(`${common.fixturesDir}/keys/agent1-key.pem`),
  cert: fs.readFileSync(`${common.fixturesDir}/keys/agent1-cert.pem`),
  ca:  fs.readFileSync(`${common.fixturesDir}/keys/ca1-cert.pem`)
};


const server = https.Server(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});


server.listen(0, function() {
  https.get({
    path: '/',
    port: this.address().port,
    rejectUnauthorized: true,
    servername: 'agent1',
    ca: options.ca
  }, function(res) {
    res.resume();
    console.log(res.statusCode);
    server.close();
  }).on('error', function(e) {
    console.log(e.message);
    process.exit(1);
  });
});

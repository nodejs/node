'use strict';
const common = require('../common');

// This test ensures that a http request callback is called
// when the agent option is set
// See https://github.com/nodejs/node-v0.x-archive/issues/1531

if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const https = require('https');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const server = https.createServer(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});

server.listen(0, common.mustCall(function() {
  console.error('listening');
  https.get({
    agent: false,
    path: '/',
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(function(res) {
    console.error(res.statusCode, res.headers);
    res.resume();
    server.close();
  })).on('error', function(e) {
    console.error(e.stack);
    process.exit(1);
  });
}));

'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

if (!common.hasMultiLocalhost()) {
  common.skip('platform-specific test.');
  return;
}

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = https.createServer(options, function(req, res) {
  console.log('Connect from: ' + req.connection.remoteAddress);
  assert.equal('127.0.0.2', req.connection.remoteAddress);

  req.on('end', function() {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('You are from: ' + req.connection.remoteAddress);
  });
  req.resume();
});

server.listen(0, '127.0.0.1', function() {
  var options = {
    host: 'localhost',
    port: this.address().port,
    path: '/',
    method: 'GET',
    localAddress: '127.0.0.2',
    rejectUnauthorized: false
  };

  var req = https.request(options, function(res) {
    res.on('end', function() {
      server.close();
      process.exit();
    });
    res.resume();
  });
  req.end();
});

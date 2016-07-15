'use strict';
var common = require('../common');
var fs = require('fs');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var invalidLocalAddress = '1.2.3.4';

var server = https.createServer(options, function(req, res) {
  console.log('Connect from: ' + req.connection.remoteAddress);

  req.on('end', function() {
    res.writeHead(200, { 'Content-Type': 'text/plain' });
    res.end('You are from: ' + req.connection.remoteAddress);
  });
  req.resume();
});

server.listen(0, '127.0.0.1', common.mustCall(function() {
  https.request({
    host: 'localhost',
    port: this.address().port,
    path: '/',
    method: 'GET',
    localAddress: invalidLocalAddress
  }, function(res) {
    common.fail('unexpectedly got response from server');
  }).on('error', common.mustCall(function(e) {
    console.log('client got error: ' + e.message);
    server.close();
  })).end();
}));

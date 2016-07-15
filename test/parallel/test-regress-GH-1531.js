'use strict';
var common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var https = require('https');

var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = https.createServer(options, function(req, res) {
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

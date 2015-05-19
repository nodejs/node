'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}

var https = require('https');
var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
  ca:  fs.readFileSync(common.fixturesDir + '/keys/ca1-cert.pem')
};


var server = https.Server(options, function(req, res) {
  res.writeHead(200);
  res.end('hello world\n');
});


server.listen(common.PORT, function() {
  https.get({
    path: '/',
    port: common.PORT,
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

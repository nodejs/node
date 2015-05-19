'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');
var http = require('http');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var https = require('https');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var body = 'hello world\n';

var httpsServer = https.createServer(options, function(req, res) {
  res.on('finish', function() {
    assert(typeof(req.connection.bytesWritten) === 'number');
    assert(req.connection.bytesWritten > 0);
    httpsServer.close();
    console.log('ok');
  });
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end(body);
});

httpsServer.listen(common.PORT, function() {
  https.get({
    port: common.PORT,
    rejectUnauthorized: false
  });
});

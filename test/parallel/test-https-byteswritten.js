'use strict';
var common = require('../common');
var assert = require('assert');
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

var body = 'hello world\n';

var httpsServer = https.createServer(options, function(req, res) {
  res.on('finish', function() {
    assert(typeof req.connection.bytesWritten === 'number');
    assert(req.connection.bytesWritten > 0);
    httpsServer.close();
    console.log('ok');
  });
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end(body);
});

httpsServer.listen(0, function() {
  https.get({
    port: this.address().port,
    rejectUnauthorized: false
  });
});

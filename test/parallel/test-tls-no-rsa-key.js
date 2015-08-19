'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/ec-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/ec-cert.pem')
};

var cert = null;

var server = tls.createServer(options, function(conn) {
  conn.end('ok');
}).listen(common.PORT, function() {
  var c = tls.connect(common.PORT, {
    rejectUnauthorized: false
  }, function() {
    cert = c.getPeerCertificate();
    c.destroy();
    server.close();
  });
});

process.on('exit', function() {
  assert(cert);
  assert.equal(cert.subject.C, 'US');
});

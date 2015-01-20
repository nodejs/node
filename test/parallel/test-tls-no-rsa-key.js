if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
var tls = require('tls');
var fs = require('fs');
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

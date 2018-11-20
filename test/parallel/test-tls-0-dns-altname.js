var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');
var net = require('net');

var common = require('../common');

var requests = 0;

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/0-dns-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/0-dns-cert.pem')
}, function(c) {
  c.once('data', function() {
    c.destroy();
    server.close();
  });
}).listen(common.PORT, function() {
  var c = tls.connect(common.PORT, {
    rejectUnauthorized: false
  }, function() {
    requests++;
    var cert = c.getPeerCertificate();
    assert.equal(cert.subjectaltname,
                 'DNS:google.com\0.evil.com, ' +
                     'DNS:just-another.com, ' +
                     'IP Address:8.8.8.8, '+
                     'IP Address:8.8.4.4, '+
                     'DNS:last.com');
    c.write('ok');
  });
});

process.on('exit', function() {
  assert.equal(requests, 1);
});

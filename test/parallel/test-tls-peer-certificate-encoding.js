var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');
var util = require('util');
var join = require('path').join;
var spawn = require('child_process').spawn;

var options = {
  key: fs.readFileSync(join(common.fixturesDir, 'keys', 'agent5-key.pem')),
  cert: fs.readFileSync(join(common.fixturesDir, 'keys', 'agent5-cert.pem')),
  ca: [ fs.readFileSync(join(common.fixturesDir, 'keys', 'ca2-cert.pem')) ]
};
var verified = false;

var server = tls.createServer(options, function(cleartext) {
  cleartext.end('World');
});
server.listen(common.PORT, function() {
  var socket = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  }, function() {
    var peerCert = socket.getPeerCertificate();

    common.debug(util.inspect(peerCert));
    assert.equal(peerCert.subject.CN, 'Ádám Lippai');
    verified = true;
    server.close();
  });
  socket.end('Hello');
});

process.on('exit', function() {
  assert.ok(verified);
});

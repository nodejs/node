'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const util = require('util');
const join = require('path').join;
const spawn = require('child_process').spawn;

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

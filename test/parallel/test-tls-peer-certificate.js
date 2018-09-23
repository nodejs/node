'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var util = require('util');
var join = require('path').join;
var spawn = require('child_process').spawn;

var options = {
  key: fs.readFileSync(join(common.fixturesDir, 'keys', 'agent1-key.pem')),
  cert: fs.readFileSync(join(common.fixturesDir, 'keys', 'agent1-cert.pem')),
  ca: [ fs.readFileSync(join(common.fixturesDir, 'keys', 'ca1-cert.pem')) ]
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
    assert.ok(!peerCert.issuerCertificate);

    // Verify that detailed return value has all certs
    peerCert = socket.getPeerCertificate(true);
    assert.ok(peerCert.issuerCertificate);

    console.error(util.inspect(peerCert));
    assert.equal(peerCert.subject.emailAddress, 'ry@tinyclouds.org');
    assert.equal(peerCert.serialNumber, '9A84ABCFB8A72AC0');
    assert.equal(peerCert.exponent, '0x10001');
    assert.deepEqual(peerCert.infoAccess['OCSP - URI'],
                     [ 'http://ocsp.nodejs.org/' ]);

    var issuer = peerCert.issuerCertificate;
    assert.ok(issuer.issuerCertificate === issuer);
    assert.equal(issuer.serialNumber, '8DF21C01468AF393');
    verified = true;
    server.close();
  });
  socket.end('Hello');
});

process.on('exit', function() {
  assert.ok(verified);
});

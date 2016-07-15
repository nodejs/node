'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var util = require('util');
var join = require('path').join;

var options = {
  key: fs.readFileSync(join(common.fixturesDir, 'keys', 'agent1-key.pem')),
  cert: fs.readFileSync(join(common.fixturesDir, 'keys', 'agent1-cert.pem')),
  ca: [ fs.readFileSync(join(common.fixturesDir, 'keys', 'ca1-cert.pem')) ]
};

var server = tls.createServer(options, function(cleartext) {
  cleartext.end('World');
});
server.listen(0, common.mustCall(function() {
  var socket = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    var peerCert = socket.getPeerCertificate();
    assert.ok(!peerCert.issuerCertificate);

    // Verify that detailed return value has all certs
    peerCert = socket.getPeerCertificate(true);
    assert.ok(peerCert.issuerCertificate);

    console.error(util.inspect(peerCert));
    assert.equal(peerCert.subject.emailAddress, 'ry@tinyclouds.org');
    assert.equal(peerCert.serialNumber, '9A84ABCFB8A72AC0');
    assert.equal(peerCert.exponent, '0x10001');
    assert.equal(peerCert.fingerprint,
                 '8D:06:3A:B3:E5:8B:85:29:72:4F:7D:1B:54:CD:95:19:3C:EF:6F:AA');
    assert.deepStrictEqual(peerCert.infoAccess['OCSP - URI'],
                           [ 'http://ocsp.nodejs.org/' ]);

    var issuer = peerCert.issuerCertificate;
    assert.ok(issuer.issuerCertificate === issuer);
    assert.equal(issuer.serialNumber, '8DF21C01468AF393');
    server.close();
  }));
  socket.end('Hello');
}));

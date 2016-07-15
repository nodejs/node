'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');

var server = tls.createServer({
  key: fs.readFileSync(common.fixturesDir + '/keys/0-dns-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/0-dns-cert.pem')
}, function(c) {
  c.once('data', function() {
    c.destroy();
    server.close();
  });
}).listen(0, common.mustCall(function() {
  var c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, common.mustCall(function() {
    var cert = c.getPeerCertificate();
    assert.equal(cert.subjectaltname,
                 'DNS:google.com\0.evil.com, ' +
                     'DNS:just-another.com, ' +
                     'IP Address:8.8.8.8, ' +
                     'IP Address:8.8.4.4, ' +
                     'DNS:last.com');
    c.write('ok');
  }));
}));

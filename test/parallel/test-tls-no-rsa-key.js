'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
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
}).listen(0, function() {
  var c = tls.connect(this.address().port, {
    rejectUnauthorized: false
  }, function() {
    c.on('end', common.mustCall(function() {
      c.end();
      server.close();
    }));

    c.on('data', function(data) {
      assert.equal(data, 'ok');
    });

    cert = c.getPeerCertificate();
  });
});

process.on('exit', function() {
  assert(cert);
  assert.equal(cert.subject.C, 'US');
});

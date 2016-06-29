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
  key: [
    fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
    fs.readFileSync(common.fixturesDir + '/keys/ec-key.pem')
  ],
  cert: [
    fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
    fs.readFileSync(common.fixturesDir + '/keys/ec-cert.pem')
  ]
};

var ciphers = [];

var server = tls.createServer(options, function(conn) {
  conn.end('ok');
}).listen(0, function() {
  var ecdsa = tls.connect(this.address().port, {
    ciphers: 'ECDHE-ECDSA-AES256-GCM-SHA384',
    rejectUnauthorized: false
  }, function() {
    ciphers.push(ecdsa.getCipher());
    var rsa = tls.connect(server.address().port, {
      ciphers: 'ECDHE-RSA-AES256-GCM-SHA384',
      rejectUnauthorized: false
    }, function() {
      ciphers.push(rsa.getCipher());
      ecdsa.end();
      rsa.end();
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.deepStrictEqual(ciphers, [{
    name: 'ECDHE-ECDSA-AES256-GCM-SHA384',
    version: 'TLSv1/SSLv3'
  }, {
    name: 'ECDHE-RSA-AES256-GCM-SHA384',
    version: 'TLSv1/SSLv3'
  }]);
});

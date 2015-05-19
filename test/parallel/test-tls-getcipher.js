'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  process.exit();
}
var tls = require('tls');

var fs = require('fs');
var cipher_list = ['RC4-SHA', 'AES256-SHA'];
var cipher_version_pattern = /TLS|SSL/;
var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem'),
  ciphers: cipher_list.join(':'),
  honorCipherOrder: true
};

var nconns = 0;

process.on('exit', function() {
  assert.equal(nconns, 1);
});

var server = tls.createServer(options, function(cleartextStream) {
  nconns++;
});

server.listen(common.PORT, '127.0.0.1', function() {
  var client = tls.connect({
    host: '127.0.0.1',
    port: common.PORT,
    ciphers: cipher_list.join(':'),
    rejectUnauthorized: false
  }, function() {
    var cipher = client.getCipher();
    assert.equal(cipher.name, cipher_list[0]);
    assert(cipher_version_pattern.test(cipher.version));
    client.end();
    server.close();
  });
});

'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
var tls = require('tls');

var fs = require('fs');
var cipher_list = ['AES128-SHA256', 'AES256-SHA256'];
var cipher_version_pattern = /TLS|SSL/;
var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem'),
  ciphers: cipher_list.join(':'),
  honorCipherOrder: true
};

var server = tls.createServer(options,
                              common.mustCall(function(cleartextStream) {}));

server.listen(0, '127.0.0.1', function() {
  var client = tls.connect({
    host: '127.0.0.1',
    port: this.address().port,
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

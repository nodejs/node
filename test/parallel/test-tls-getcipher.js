'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const cipher_list = ['AES128-SHA256', 'AES256-SHA256'];
const cipher_version_pattern = /TLS|SSL/;
const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent2-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent2-cert.pem'),
  ciphers: cipher_list.join(':'),
  honorCipherOrder: true
};

const server = tls.createServer(options,
                              common.mustCall(function(cleartextStream) {}));

server.listen(0, '127.0.0.1', function() {
  const client = tls.connect({
    host: '127.0.0.1',
    port: this.address().port,
    ciphers: cipher_list.join(':'),
    rejectUnauthorized: false
  }, function() {
    const cipher = client.getCipher();
    assert.equal(cipher.name, cipher_list[0]);
    assert(cipher_version_pattern.test(cipher.version));
    client.end();
    server.close();
  });
});

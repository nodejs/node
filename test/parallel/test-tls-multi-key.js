'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');
const assert = require('assert');
const tls = require('tls');

const options = {
  key: [
    fixtures.readKey('ec-key.pem'),
    fixtures.readKey('agent1-key.pem'),
  ],
  cert: [
    fixtures.readKey('agent1-cert.pem'),
    fixtures.readKey('ec-cert.pem'),
  ]
};

const ciphers = [];

const server = tls.createServer(options, function(conn) {
  conn.end('ok');
}).listen(0, function() {
  const ecdsa = tls.connect(this.address().port, {
    ciphers: 'ECDHE-ECDSA-AES256-GCM-SHA384',
    rejectUnauthorized: false
  }, function() {
    ciphers.push(ecdsa.getCipher());
    const rsa = tls.connect(server.address().port, {
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

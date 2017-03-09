'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');
const fs = require('fs');

const options = {
  key: [
    fs.readFileSync(common.fixturesDir + '/keys/ec-key.pem'),
    fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  ],
  cert: [
    fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem'),
    fs.readFileSync(common.fixturesDir + '/keys/ec-cert.pem')
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

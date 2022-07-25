'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const options = {
  pfx: [
    {
      buf: fixtures.readKey('agent1.pfx'),
      passphrase: 'sample'
    },
    fixtures.readKey('ec.pfx'),
  ]
};

const ciphers = [];

const server = tls.createServer(options, function(conn) {
  conn.end('ok');
}).listen(0, function() {
  const ecdsa = tls.connect(this.address().port, {
    ciphers: 'ECDHE-ECDSA-AES256-GCM-SHA384',
    maxVersion: 'TLSv1.2',
    rejectUnauthorized: false,
  }, common.mustCall(function() {
    ciphers.push(ecdsa.getCipher());
    const rsa = tls.connect(server.address().port, {
      ciphers: 'ECDHE-RSA-AES256-GCM-SHA384',
      maxVersion: 'TLSv1.2',
      rejectUnauthorized: false,
    }, common.mustCall(function() {
      ciphers.push(rsa.getCipher());
      ecdsa.end();
      rsa.end();
      server.close();
    }));
  }));
});

process.on('exit', function() {
  assert.deepStrictEqual(ciphers, [{
    name: 'ECDHE-ECDSA-AES256-GCM-SHA384',
    standardName: 'TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384',
    version: 'TLSv1.2'
  }, {
    name: 'ECDHE-RSA-AES256-GCM-SHA384',
    standardName: 'TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384',
    version: 'TLSv1.2'
  }]);
});

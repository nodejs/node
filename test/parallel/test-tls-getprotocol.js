'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { hasOpenSSL, hasFIPS } = require('../common/crypto');

// This test ensures that `getProtocol` returns the right protocol
// from a TLS connection

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const fips3 = hasFIPS(3);

let clientConfigs = [
  {
    secureProtocol: 'TLSv1_method',
    version: 'TLSv1',
    ciphers: (hasOpenSSL(3, 1) ? 'DEFAULT:@SECLEVEL=0' : 'DEFAULT')
  }, {
    secureProtocol: 'TLSv1_1_method',
    version: 'TLSv1.1',
    ciphers: (hasOpenSSL(3, 1) ? 'DEFAULT:@SECLEVEL=0' : 'DEFAULT')
  }, {
    secureProtocol: 'TLSv1_2_method',
    version: 'TLSv1.2'
  },
];

if (process.features.openssl_is_boringssl) {
  // Remove the TLSv1 and TLSv1.1 cases. BoringSSL does not negotiate those
  // legacy protocols in this configuration; keep TLSv1.2 to cover getProtocol()
  // on a successful BoringSSL TLS handshake.
  common.printSkipMessage('BoringSSL: skipping TLSv1/TLSv1.1 getProtocol cases');
  clientConfigs = clientConfigs.filter(({ version }) => version === 'TLSv1.2');
}

const serverConfig = {
  secureProtocol: 'TLS_method',
  key: fixtures.readKey('agent2-key.pem'),
  cert: fixtures.readKey('agent2-cert.pem')
};

if (!process.features.openssl_is_boringssl) {
  serverConfig.ciphers = fips3 ?
    'ECDHE-RSA-AES256-GCM-SHA384' : 'RSA@SECLEVEL=0';
}

const expectedConnections = fips3 ? 1 : clientConfigs.length;
const server = tls.createServer(serverConfig, common.mustCall(expectedConnections));

if (fips3) {
  server.on('tlsClientError', common.mustCall((err) => {
    assert.ok([
      'ERR_SSL_NO_SUITABLE_DIGEST_ALGORITHM',
      'ERR_SSL_UNEXPECTED_MESSAGE',
    ].includes(err.code), err);
  }, 2));
}

server
.listen(0, common.localhostIPv4, common.mustCall(function() {
  let completed = 0;
  function done() {
    if (++completed === clientConfigs.length)
      server.close();
  }

  for (const v of clientConfigs) {
    const shouldConnect = !fips3 || v.version === 'TLSv1.2';
    const client = tls.connect({
      host: common.localhostIPv4,
      port: server.address().port,
      ciphers: v.ciphers,
      rejectUnauthorized: false,
      secureProtocol: v.secureProtocol
    }, shouldConnect ? common.mustCall(function() {
      assert.strictEqual(this.getProtocol(), v.version);
      this.on('end', common.mustCall());
      this.on('close', common.mustCall(function() {
        assert.strictEqual(this.getProtocol(), null);
        done();
      })).end();
    }) : common.mustNotCall());

    if (!shouldConnect) {
      client.on('error', common.mustCall((err) => assert(err.code)));
      client.on('close', common.mustCall(done));
    }
  }
}));

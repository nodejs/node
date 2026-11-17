'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('non-canonical-ip-dns-key.pem');
const cert = fixtures.readKey('non-canonical-ip-dns-cert.pem');
const ca = fixtures.readKey('ca1-cert.pem');
const server = tls.createServer({ key, cert }, (socket) => socket.end());

server.listen(0, '127.0.0.1', common.mustCall(() => {
  const client = tls.connect({
    host: '0127.0.0.1',
    port: server.address().port,
    ca,
    rejectUnauthorized: true,
    lookup: common.mustCall((hostname, options, callback) => {
      assert.strictEqual(hostname, '0127.0.0.1');
      assert.strictEqual(options.all, true);
      callback(null, [{ address: '127.0.0.1', family: 4 }]);
    }),
  }, common.mustNotCall());

  client.on('error', common.mustCall((error) => {
    assert.strictEqual(error.code, 'ERR_TLS_CERT_ALTNAME_INVALID');
    server.close();
  }));
}));

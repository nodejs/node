'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');

const assert = require('assert');
const tls = require('tls');

const key = fixtures.readKey('agent2-key.pem');
const cert = fixtures.readKey('agent2-cert.pem');

// TODO(@sam-github) test works with TLS1.3, rework test to add
//   'ECDH' with 'TLS_AES_128_GCM_SHA256',

function loadDHParam(n) {
  return fixtures.readKey(`dh${n}.pem`);
}

function test(size, type, name, cipher) {
  assert(cipher);

  const options = {
    key: key,
    cert: cert,
    ciphers: cipher
  };

  if (name) options.ecdhCurve = name;

  if (type === 'DH') options.dhparam = loadDHParam(size);

  const server = tls.createServer(options, common.mustCall((conn) => {
    assert.strictEqual(conn.getEphemeralKeyInfo(), null);
    conn.end();
  }));

  server.on('close', common.mustCall((err) => {
    assert.ifError(err);
  }));

  server.listen(0, '127.0.0.1', common.mustCall(() => {
    const client = tls.connect({
      port: server.address().port,
      rejectUnauthorized: false
    }, common.mustCall(function() {
      const ekeyinfo = client.getEphemeralKeyInfo();
      assert.strictEqual(ekeyinfo.type, type);
      assert.strictEqual(ekeyinfo.size, size);
      assert.strictEqual(ekeyinfo.name, name);
      server.close();
    }));
    client.on('secureConnect', common.mustCall());
  }));
}

test(undefined, undefined, undefined, 'AES128-SHA256');
test(1024, 'DH', undefined, 'DHE-RSA-AES128-GCM-SHA256');
test(2048, 'DH', undefined, 'DHE-RSA-AES128-GCM-SHA256');
test(256, 'ECDH', 'prime256v1', 'ECDHE-RSA-AES128-GCM-SHA256');
test(521, 'ECDH', 'secp521r1', 'ECDHE-RSA-AES128-GCM-SHA256');
test(253, 'ECDH', 'X25519', 'ECDHE-RSA-AES128-GCM-SHA256');
test(448, 'ECDH', 'X448', 'ECDHE-RSA-AES128-GCM-SHA256');

'use strict';

// Test return value of tlsSocket.exportKeyingMaterial

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const net = require('net');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');

const server = net.createServer(common.mustCall((s) => {
  const tlsSocket = new tls.TLSSocket(s, {
    isServer: true,
    server: server,
    secureContext: tls.createSecureContext({ key, cert })
  });

  assert.throws(() => {
    tlsSocket.exportKeyingMaterial(128, 'label');
  }, {
    name: 'Error',
    message: 'TLS socket connection must be securely established',
    code: 'ERR_TLS_INVALID_STATE'
  });

  tlsSocket.on('secure', common.mustCall(() => {
    const label = 'client finished';

    const validKeyingMaterial = tlsSocket.exportKeyingMaterial(128, label);
    assert.strictEqual(validKeyingMaterial.length, 128);

    const validKeyingMaterialWithContext = tlsSocket
      .exportKeyingMaterial(128, label, Buffer.from([0, 1, 2, 3]));
    assert.strictEqual(validKeyingMaterialWithContext.length, 128);

    // Ensure providing a context results in a different key than without
    assert.notStrictEqual(validKeyingMaterial, validKeyingMaterialWithContext);

    const validKeyingMaterialWithEmptyContext = tlsSocket
      .exportKeyingMaterial(128, label, Buffer.from([]));
    assert.strictEqual(validKeyingMaterialWithEmptyContext.length, 128);

    assert.throws(() => {
      tlsSocket.exportKeyingMaterial(128, label, 'stringAsContextNotSupported');
    }, {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE'
    });

    assert.throws(() => {
      tlsSocket.exportKeyingMaterial(128, label, 1234);
    }, {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE'
    });

    assert.throws(() => {
      tlsSocket.exportKeyingMaterial(10, null);
    }, {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE'
    });

    assert.throws(() => {
      tlsSocket.exportKeyingMaterial('length', 1234);
    }, {
      name: 'TypeError',
      code: 'ERR_INVALID_ARG_TYPE'
    });

    assert.throws(() => {
      tlsSocket.exportKeyingMaterial(-3, 'a');
    }, {
      name: 'RangeError',
      code: 'ERR_OUT_OF_RANGE'
    });

    assert.throws(() => {
      tlsSocket.exportKeyingMaterial(0, 'a');
    }, {
      name: 'RangeError',
      code: 'ERR_OUT_OF_RANGE'
    });

    tlsSocket.end();
    server.close();
  }));
})).listen(0, () => {
  const opts = {
    port: server.address().port,
    rejectUnauthorized: false
  };

  tls.connect(opts, common.mustCall(function() { this.end(); }));
});

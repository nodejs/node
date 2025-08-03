// Flags: --experimental-quic --no-warnings
'use strict';

const common = require('../common');
const assert = require('node:assert');

if (!common.hasQuic) {
  common.skip('QUIC is not enabled');
}

const { createPrivateKey } = require('node:crypto');
const fixtures = require('../common/fixtures');
const keys = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const certs = fixtures.readKey('agent1-cert.pem');

const {
  listen,
  connect,
} = require('node:quic');

const p1 = Promise.withResolvers();
const p2 = Promise.withResolvers();

(async () => {
  const serverEndpoint = await listen((serverSession) => {

    serverSession.opened.then((info) => {
      assert.strictEqual(info.servername, 'localhost');
      assert.strictEqual(info.protocol, 'h3');
      assert.strictEqual(info.cipher, 'TLS_AES_128_GCM_SHA256');

      p1.resolve();
      serverSession.close();
    }).then(common.mustCall());
  }, { keys, certs });

  assert.ok(serverEndpoint.address !== undefined);

  const clientSession = await connect(serverEndpoint.address);
  clientSession.opened.then((info) => {
    assert.strictEqual(info.servername, 'localhost');
    assert.strictEqual(info.protocol, 'h3');
    assert.strictEqual(info.cipher, 'TLS_AES_128_GCM_SHA256');
    p2.resolve();
  }).then(common.mustCall());

  await Promise.all([p1.promise, p2.promise]);
  clientSession.close();
})().then(common.mustCall());

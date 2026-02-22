// Flags: --experimental-quic --no-warnings
'use strict';

const { hasQuic, mustCall } = require('../common');
const { Buffer } = require('node:buffer');

const {
  describe,
  it,
} = require('node:test');

// TODO(@jasnell): Temporarily skip the test on mac until we can figure
// out while it is failing on macs in CI but running locally on macs ok.
const isMac = process.platform === 'darwin';
const skip = isMac || !hasQuic;

async function readAll(readable, resolve) {
  const chunks = [];
  for await (const chunk of readable) {
    chunks.push(chunk);
  }
  resolve(Buffer.concat(chunks));
}

describe('quic basic server/client handshake works', { skip }, async () => {
  const { createPrivateKey } = require('node:crypto');
  const fixtures = require('../common/fixtures');
  const keys = createPrivateKey(fixtures.readKey('agent1-key.pem'));
  const certs = fixtures.readKey('agent1-cert.pem');

  const {
    listen,
    connect,
  } = require('node:quic');

  const assert = require('node:assert');

  it('a quic client can connect to a quic server in the same process', async () => {
    const p1 = Promise.withResolvers();
    const p2 = Promise.withResolvers();
    const p3 = Promise.withResolvers();

    const serverEndpoint = await listen(mustCall((serverSession) => {

      serverSession.opened.then((info) => {
        assert.strictEqual(info.servername, 'localhost');
        assert.strictEqual(info.protocol, 'h3');
        assert.strictEqual(info.cipher, 'TLS_AES_128_GCM_SHA256');
        p1.resolve();
      }).then(mustCall());

      serverSession.onstream = mustCall((stream) => {
        readAll(stream.readable, p3.resolve).then(() => {
          serverSession.close();
        }).then(mustCall());
      });
    }), { keys, certs });

    assert.ok(serverEndpoint.address !== undefined);

    const clientSession = await connect(serverEndpoint.address);
    clientSession.opened.then((info) => {
      assert.strictEqual(info.servername, 'localhost');
      assert.strictEqual(info.protocol, 'h3');
      assert.strictEqual(info.cipher, 'TLS_AES_128_GCM_SHA256');
      p2.resolve();
    }).then(mustCall());

    const body = new Blob(['hello']);
    const stream = await clientSession.createUnidirectionalStream({
      body,
    });
    assert.ok(stream);

    const { 2: data } = await Promise.all([p1.promise, p2.promise, p3.promise]);
    clientSession.close();
    assert.strictEqual(Buffer.from(data).toString(), 'hello');
  });
});

// Flags: --expose-internals --no-warnings
'use strict';

const { hasQuic } = require('../common');

const {
  describe,
  it,
} = require('node:test');

describe('quic internal endpoint listen defaults', { skip: !hasQuic }, async () => {
  const assert = require('node:assert');

  const {
    kState,
  } = require('internal/quic/symbols');

  const { createPrivateKey } = require('node:crypto');
  const fixtures = require('../common/fixtures');
  const keys = createPrivateKey(fixtures.readKey('agent1-key.pem'));
  const certs = fixtures.readKey('agent1-cert.pem');

  const {
    SocketAddress,
  } = require('net');

  const {
    QuicEndpoint,
    listen,
  } = require('internal/quic/quic');

  it('are reasonable and work as expected', async () => {
    const endpoint = new QuicEndpoint();

    assert.ok(!endpoint[kState].isBound);
    assert.ok(!endpoint[kState].isReceiving);
    assert.ok(!endpoint[kState].isListening);

    assert.strictEqual(endpoint.address, undefined);

    await assert.rejects(listen(123, { keys, certs, endpoint }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });

    await assert.rejects(listen(() => {}, 123), {
      code: 'ERR_INVALID_ARG_TYPE',
    });

    await listen(() => {}, { keys, certs, endpoint });
    await assert.rejects(listen(() => {}, { keys, certs, endpoint }), {
      code: 'ERR_INVALID_STATE',
    });

    assert.ok(endpoint[kState].isBound);
    assert.ok(endpoint[kState].isReceiving);
    assert.ok(endpoint[kState].isListening);

    const address = endpoint.address;
    assert.ok(address instanceof SocketAddress);

    assert.strictEqual(address.address, '127.0.0.1');
    assert.strictEqual(address.family, 'ipv4');
    assert.strictEqual(address.flowlabel, 0);
    assert.ok(address.port !== 0);

    assert.ok(!endpoint.destroyed);
    endpoint.destroy();
    assert.strictEqual(endpoint.closed, endpoint.close());
    await endpoint.closed;
    assert.ok(endpoint.destroyed);

    await assert.rejects(listen(() => {}, { keys, certs, endpoint }), {
      code: 'ERR_INVALID_STATE',
    });
    assert.throws(() => { endpoint.busy = true; }, {
      code: 'ERR_INVALID_STATE',
    });
    await endpoint[Symbol.asyncDispose]();

    assert.strictEqual(endpoint.address, undefined);
  });
});

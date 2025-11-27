// Flags: --expose-internals --experimental-quic --no-warnings
import { hasQuic, skip } from '../common/index.mjs';

import assert from 'node:assert';
import { readKey } from '../common/fixtures.mjs';
import { SocketAddress } from 'node:net';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

// Import after the hasQuic check
const { listen, QuicEndpoint } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { getQuicEndpointState } = (await import('internal/quic/quic')).default;

const keys = createPrivateKey(readKey('agent1-key.pem'));
const certs = readKey('agent1-cert.pem');

const endpoint = new QuicEndpoint();
const state = getQuicEndpointState(endpoint);
assert.ok(!state.isBound);
assert.ok(!state.isReceiving);
assert.ok(!state.isListening);

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
assert.ok(state.isBound);
assert.ok(state.isReceiving);
assert.ok(state.isListening);

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

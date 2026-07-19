// Flags: --expose-internals --experimental-quic --no-warnings
import { hasQuic, skip, mustNotCall } from '../common/index.mjs';

import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;
import { SocketAddress } from 'node:net';

const { strictEqual, rejects, ok, throws } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

// Import after the hasQuic check
const { listen, QuicEndpoint } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { getQuicEndpointState } = (await import('internal/quic/quic')).default;

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const sni = { '*': { keys: [key], certs: [cert] } };

const endpoint = new QuicEndpoint();
const state = getQuicEndpointState(endpoint);
ok(!state.isBound);
ok(!state.isReceiving);
ok(!state.isListening);

strictEqual(endpoint.address, undefined);

await rejects(listen(123, { sni, endpoint }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
// Buffer is not detached.
strictEqual(cert.buffer.detached, false);

await rejects(listen(mustNotCall(), 123), {
  code: 'ERR_INVALID_ARG_TYPE',
});

await listen(mustNotCall(), { sni, endpoint });
// Buffer is not detached.
strictEqual(cert.buffer.detached, false);

await rejects(listen(mustNotCall(), { sni, endpoint }), {
  code: 'ERR_INVALID_STATE',
});
// Buffer is not detached.
strictEqual(cert.buffer.detached, false);

ok(state.isBound);
ok(state.isReceiving);
ok(state.isListening);

const address = endpoint.address;
ok(address instanceof SocketAddress);

strictEqual(address.address, '127.0.0.1');
strictEqual(address.family, 'ipv4');
strictEqual(address.flowlabel, 0);
ok(address.port !== 0);

ok(!endpoint.destroyed);
endpoint.destroy();
strictEqual(endpoint.closed, endpoint.close());
await endpoint.closed;
ok(endpoint.destroyed);

await rejects(listen(mustNotCall(), { sni, endpoint }), {
  code: 'ERR_INVALID_STATE',
});
// Buffer is not detached.
strictEqual(cert.buffer.detached, false);

throws(() => { endpoint.busy = true; }, {
  code: 'ERR_INVALID_STATE',
});
await endpoint[Symbol.asyncDispose]();

strictEqual(endpoint.address, undefined);

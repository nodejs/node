// Flags: --expose-internals --experimental-quic --no-warnings
import { hasQuic, skip } from '../common/index.mjs';

import {
  ok,
  rejects,
  strictEqual,
  throws,
} from 'node:assert';
import { readKey } from '../common/fixtures.mjs';
import { SocketAddress } from 'node:net';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

// Import after the hasQuic check
const { listen, QuicEndpoint } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { kState } = (await import('internal/quic/symbols')).default;

const keys = createPrivateKey(readKey('agent1-key.pem'));
const certs = readKey('agent1-cert.pem');

const endpoint = new QuicEndpoint();

ok(!endpoint[kState].isBound);
ok(!endpoint[kState].isReceiving);
ok(!endpoint[kState].isListening);

strictEqual(endpoint.address, undefined);

await rejects(listen(123, { keys, certs, endpoint }), {
  code: 'ERR_INVALID_ARG_TYPE',
});

await rejects(listen(() => {}, 123), {
  code: 'ERR_INVALID_ARG_TYPE',
});

await listen(() => {}, { keys, certs, endpoint });
await rejects(listen(() => {}, { keys, certs, endpoint }), {
  code: 'ERR_INVALID_STATE',
});

ok(endpoint[kState].isBound);
ok(endpoint[kState].isReceiving);
ok(endpoint[kState].isListening);

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

await rejects(listen(() => {}, { keys, certs, endpoint }), {
  code: 'ERR_INVALID_STATE',
});
throws(() => { endpoint.busy = true; }, {
  code: 'ERR_INVALID_STATE',
});
await endpoint[Symbol.asyncDispose]();

strictEqual(endpoint.address, undefined);

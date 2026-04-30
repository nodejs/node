// Flags: --experimental-quic --no-warnings

// Test: validateAddress triggers Retry flow.
// When the server endpoint has validateAddress: true, it should send
// a Retry packet before accepting the connection. The handshake still
// completes successfully.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, QuicEndpoint } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const endpoint = new QuicEndpoint({ validateAddress: true });

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  const info = await serverSession.opened;
  // The handshake should complete despite the Retry flow.
  strictEqual(info.protocol, 'quic-test');
  serverSession.close();
}), {
  endpoint,
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
});

const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  servername: 'localhost',
});

const info = await clientSession.opened;
strictEqual(info.protocol, 'quic-test');

// The serverEndpoint must be closed after we wait for the clientSession to close.
await clientSession.closed;
await serverEndpoint.close();

// Flags: --experimental-quic --no-warnings

// Test: session.opened resolves with handshake info (INFO-05, INFO-06,
// INFO-07, INFO-08).
// local and remote SocketAddress objects are correct.
// servername matches the SNI sent by the client.
// protocol matches the negotiated ALPN.
// cipher and cipherVersion reflect the negotiated cipher suite.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  const info = await serverSession.opened;

  // Server sees its own local address and the client's remote.
  assert.strictEqual(info.local.address, '127.0.0.1');
  assert.strictEqual(info.local.family, 'ipv4');
  assert.strictEqual(typeof info.local.port, 'number');
  assert.strictEqual(info.remote.address, '127.0.0.1');
  assert.strictEqual(info.remote.family, 'ipv4');
  assert.strictEqual(typeof info.remote.port, 'number');

  // Local and remote ports should differ.
  assert.notStrictEqual(info.local.port, info.remote.port);

  // Servername matches the SNI.
  assert.strictEqual(info.servername, 'localhost');

  // Protocol matches ALPN.
  assert.strictEqual(info.protocol, 'quic-test');

  // cipher info.
  assert.strictEqual(typeof info.cipher, 'string');
  assert.ok(info.cipher.length > 0);
  assert.strictEqual(info.cipherVersion, 'TLSv1.3');

  serverSession.close();
  serverDone.resolve();
}));

const clientSession = await connect(serverEndpoint.address);
const clientInfo = await clientSession.opened;

// Client sees its own local address and the server's remote.
assert.strictEqual(clientInfo.local.address, '127.0.0.1');
assert.strictEqual(clientInfo.remote.address, '127.0.0.1');
assert.notStrictEqual(clientInfo.local.port, clientInfo.remote.port);

// servername matches.
assert.strictEqual(clientInfo.servername, 'localhost');

// Protocol matches ALPN.
assert.strictEqual(clientInfo.protocol, 'quic-test');

// cipher info.
assert.strictEqual(typeof clientInfo.cipher, 'string');
assert.ok(clientInfo.cipher.length > 0);
assert.strictEqual(clientInfo.cipherVersion, 'TLSv1.3');

await Promise.all([serverDone.promise, clientSession.closed]);
await serverEndpoint.close();

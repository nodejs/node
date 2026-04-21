// Flags: --experimental-quic --no-warnings

// Test: session.opened resolves with handshake info (INFO-05, INFO-06,
// INFO-07, INFO-08).
// local and remote SocketAddress objects are correct.
// servername matches the SNI sent by the client.
// protocol matches the negotiated ALPN.
// cipher and cipherVersion reflect the negotiated cipher suite.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { notStrictEqual, strictEqual, ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  const info = await serverSession.opened;

  // Server sees its own local address and the client's remote.
  strictEqual(info.local.address, '127.0.0.1');
  strictEqual(info.local.family, 'ipv4');
  strictEqual(typeof info.local.port, 'number');
  strictEqual(info.remote.address, '127.0.0.1');
  strictEqual(info.remote.family, 'ipv4');
  strictEqual(typeof info.remote.port, 'number');

  // Local and remote ports should differ.
  notStrictEqual(info.local.port, info.remote.port);

  // Servername matches the SNI.
  strictEqual(info.servername, 'localhost');

  // Protocol matches ALPN.
  strictEqual(info.protocol, 'quic-test');

  // cipher info.
  strictEqual(typeof info.cipher, 'string');
  ok(info.cipher.length > 0);
  strictEqual(info.cipherVersion, 'TLSv1.3');

  serverSession.close();
  serverDone.resolve();
}));

const clientSession = await connect(serverEndpoint.address);
const clientInfo = await clientSession.opened;

// Client sees its own local address and the server's remote.
strictEqual(clientInfo.local.address, '127.0.0.1');
strictEqual(clientInfo.remote.address, '127.0.0.1');
notStrictEqual(clientInfo.local.port, clientInfo.remote.port);

// servername matches.
strictEqual(clientInfo.servername, 'localhost');

// Protocol matches ALPN.
strictEqual(clientInfo.protocol, 'quic-test');

// cipher info.
strictEqual(typeof clientInfo.cipher, 'string');
ok(clientInfo.cipher.length > 0);
strictEqual(clientInfo.cipherVersion, 'TLSv1.3');

await Promise.all([serverDone.promise, clientSession.closed]);
await serverEndpoint.close();

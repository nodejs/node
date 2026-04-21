// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

// Import after the hasQuic check
const quic = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const keys = createPrivateKey(readKey('agent1-key.pem'));
const certs = readKey('agent1-cert.pem');

const serverDone = Promise.withResolvers();

// Create a server endpoint
const serverEndpoint = await quic.listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  ok(serverSession.endpoint !== null);
  strictEqual(serverSession.destroyed, false);

  const stats = serverSession.stats;
  strictEqual(stats.isConnected, true);
  ok(stats.handshakeCompletedAt > 0n);
  ok(stats.handshakeConfirmedAt > 0n);
  strictEqual(stats.closingAt, 0n);

  serverDone.resolve();
  serverSession.close();
}), { sni: { '*': { keys, certs } } });

strictEqual(serverEndpoint.busy, false);
strictEqual(serverEndpoint.closing, false);
strictEqual(serverEndpoint.destroyed, false);
strictEqual(serverEndpoint.listening, true);

ok(serverEndpoint.address !== undefined);
strictEqual(serverEndpoint.address.family, 'ipv4');
strictEqual(serverEndpoint.address.address, '127.0.0.1');
ok(typeof serverEndpoint.address.port === 'number');
ok(serverEndpoint.address.port > 0);

const epStats = serverEndpoint.stats;
strictEqual(epStats.isConnected, true);
ok(epStats.createdAt > 0n);

// Connect with a client
const clientSession = await quic.connect(serverEndpoint.address);

strictEqual(clientSession.destroyed, false);
ok(clientSession.endpoint !== null);
strictEqual(clientSession.stats.isConnected, true);

const clientInfo = await clientSession.opened;
strictEqual(clientInfo.servername, 'localhost');
strictEqual(clientInfo.protocol, 'h3');
strictEqual(clientInfo.cipherVersion, 'TLSv1.3');
ok(clientInfo.local !== undefined);
ok(clientInfo.remote !== undefined);

const cStats = clientSession.stats;
strictEqual(cStats.isConnected, true);
ok(cStats.handshakeCompletedAt > 0n);
ok(cStats.bytesSent > 0n, 'Expected bytesSent > 0 after handshake');

await serverDone.promise;

// Open a bidirectional stream.
const stream = await clientSession.createBidirectionalStream();

strictEqual(stream.destroyed, false);
strictEqual(stream.direction, 'bidi');
strictEqual(stream.session, clientSession);
ok(stream.id !== null, 'Non-pending stream should have an id');
strictEqual(typeof stream.id, 'bigint');
strictEqual(stream.pending, false);
strictEqual(stream.stats.isConnected, true);

// Destroying the session should destroy it and the stream, and clear its properties.
clientSession.destroy();
strictEqual(clientSession.destroyed, true);
strictEqual(clientSession.endpoint, null);
strictEqual(clientSession.stats.isConnected, false);

strictEqual(stream.destroyed, true);
strictEqual(stream.session, null);
strictEqual(stream.id, null);
strictEqual(stream.direction, null);

await serverEndpoint.close();

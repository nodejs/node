// Flags: --experimental-quic --no-warnings

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

// Import after the hasQuic check
const quic = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const keys = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const certs = fixtures.readKey('agent1-cert.pem');

const serverDone = Promise.withResolvers();
const clientDone = Promise.withResolvers();

// Create a server endpoint
const serverEndpoint = await quic.listen(mustCall((serverSession) => {
  serverSession.opened.then((info) => {
    assert.ok(serverSession.endpoint !== null);
    assert.strictEqual(serverSession.destroyed, false);

    const stats = serverSession.stats;
    assert.strictEqual(stats.isConnected, true);
    assert.ok(stats.handshakeCompletedAt > 0n);
    assert.ok(stats.handshakeConfirmedAt > 0n);
    assert.strictEqual(stats.closingAt, 0n);

    serverDone.resolve();
    serverSession.close();
  }).then(mustCall());
}), { sni: { '*': { keys, certs } } });

assert.strictEqual(serverEndpoint.busy, false);
assert.strictEqual(serverEndpoint.closing, false);
assert.strictEqual(serverEndpoint.destroyed, false);
assert.strictEqual(serverEndpoint.listening, true);

assert.ok(serverEndpoint.address !== undefined);
assert.strictEqual(serverEndpoint.address.family, 'ipv4');
assert.strictEqual(serverEndpoint.address.address, '127.0.0.1');
assert.ok(typeof serverEndpoint.address.port === 'number');
assert.ok(serverEndpoint.address.port > 0);

const epStats = serverEndpoint.stats;
assert.strictEqual(epStats.isConnected, true);
assert.ok(epStats.createdAt > 0n);

// Connect with a client
const clientSession = await quic.connect(serverEndpoint.address);

assert.strictEqual(clientSession.destroyed, false);
assert.ok(clientSession.endpoint !== null);
assert.strictEqual(clientSession.stats.isConnected, true);

clientSession.opened.then((clientInfo) => {
  assert.strictEqual(clientInfo.servername, 'localhost');
  assert.strictEqual(clientInfo.protocol, 'h3');
  assert.strictEqual(clientInfo.cipherVersion, 'TLSv1.3');
  assert.ok(clientInfo.local !== undefined);
  assert.ok(clientInfo.remote !== undefined);

  const cStats = clientSession.stats;
  assert.strictEqual(cStats.isConnected, true);
  assert.ok(cStats.handshakeCompletedAt > 0n);
  assert.ok(cStats.bytesSent > 0n, 'Expected bytesSent > 0 after handshake');

  clientDone.resolve();
}).then(mustCall());

await Promise.all([serverDone.promise, clientDone.promise]);

// Open a bidirectional stream.
const stream = await clientSession.createBidirectionalStream();

assert.strictEqual(stream.destroyed, false);
assert.strictEqual(stream.direction, 'bidi');
assert.strictEqual(stream.session, clientSession);
assert.ok(stream.id !== null, 'Non-pending stream should have an id');
assert.strictEqual(typeof stream.id, 'bigint');
assert.strictEqual(stream.pending, false);
assert.strictEqual(stream.stats.isConnected, true);
assert.ok(stream.readable instanceof ReadableStream);

// Destroying the session should destroy it and the stream, and clear its properties.
clientSession.destroy();
assert.strictEqual(clientSession.destroyed, true);
assert.strictEqual(clientSession.endpoint, null);
assert.strictEqual(clientSession.stats.isConnected, false);

assert.strictEqual(stream.destroyed, true);
assert.strictEqual(stream.session, null);
assert.strictEqual(stream.id, null);
assert.strictEqual(stream.direction, null);

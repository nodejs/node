// Flags: --experimental-dtls --no-warnings

// Test: DTLS endpoint and session stats increment with data transfer.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual, notStrictEqual } = assert;
const { readKey } = fixtures;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const serverCert = readKey('agent1-cert.pem');
const serverKey = readKey('agent1-key.pem');
const ca = readKey('ca1-cert.pem');

const serverReceivedData = Promise.withResolvers();
const clientReceivedData = Promise.withResolvers();

let serverSession;

// Start server.
const endpoint = listen(mustCall((session) => {
  serverSession = session;

  session.onmessage = mustCall((data) => {
    strictEqual(data.toString(), 'hello from client');
    session.send('hello from server');
    serverReceivedData.resolve();
  });

  session.onhandshake = mustCall();
}), {
  cert: serverCert.toString(),
  key: serverKey.toString(),
  port: 0,
  host: '127.0.0.1',
});

// --- Endpoint stats should be available immediately ---

const epStats = endpoint.stats;
ok(epStats, 'endpoint.stats should be defined');
ok(epStats.isConnected, 'stats should be connected');
ok(epStats.createdAt > 0n, 'createdAt should be set');
strictEqual(epStats.destroyedAt, 0n);
strictEqual(epStats.clientSessions, 0n);
strictEqual(epStats.serverSessions, 0n);
strictEqual(epStats.serverBusyCount, 0n);

// Connect client.
const clientSession = connect('127.0.0.1', endpoint.address.port, {
  ca: [ca.toString()],
  rejectUnauthorized: false,
});

clientSession.onmessage = mustCall((data) => {
  strictEqual(data.toString(), 'hello from server');
  clientReceivedData.resolve();
});

clientSession.onhandshake = mustCall();

// Wait for handshake.
await clientSession.opened;

// --- Client session stats after handshake ---

const csStats = clientSession.stats;
ok(csStats, 'session.stats should be defined');
ok(csStats.isConnected, 'session stats should be connected');
ok(csStats.createdAt > 0n, 'createdAt should be set');
ok(csStats.handshakeCompletedAt > 0n, 'handshake timestamp should be set');
ok(csStats.handshakeCompletedAt >= csStats.createdAt,
   'handshake should complete after creation');
strictEqual(csStats.closingAt, 0n);
strictEqual(csStats.destroyedAt, 0n);

// Record bytes before sending application data.
const csBytesSentBefore = csStats.bytesSent;
const csMessagesSentBefore = csStats.messagesSent;

// Send data.
clientSession.send('hello from client');

// Wait for bidirectional exchange.
await Promise.all([serverReceivedData.promise, clientReceivedData.promise]);

// --- Client session stats after data exchange ---

ok(csStats.bytesSent > csBytesSentBefore,
   'bytesSent should increase after send');
ok(csStats.messagesSent > csMessagesSentBefore,
   'messagesSent should increase after send');
ok(csStats.bytesReceived > 0n, 'bytesReceived should be non-zero');
ok(csStats.messagesReceived > 0n, 'messagesReceived should be non-zero');

// --- Server session stats after data exchange ---

ok(serverSession, 'server session should exist');
const ssStats = serverSession.stats;
ok(ssStats.bytesReceived > 0n, 'server bytesReceived should be non-zero');
ok(ssStats.messagesReceived > 0n, 'server messagesReceived should be non-zero');
ok(ssStats.bytesSent > 0n, 'server bytesSent should be non-zero');
ok(ssStats.messagesSent > 0n, 'server messagesSent should be non-zero');
ok(ssStats.handshakeCompletedAt > 0n, 'server handshake timestamp should be set');

// --- Endpoint stats after data exchange ---

ok(epStats.bytesReceived > 0n, 'endpoint bytesReceived should be non-zero');
ok(epStats.bytesSent > 0n, 'endpoint bytesSent should be non-zero');
ok(epStats.packetsReceived > 0n, 'endpoint packetsReceived should be non-zero');
ok(epStats.packetsSent > 0n, 'endpoint packetsSent should be non-zero');
strictEqual(epStats.serverSessions, 1n);

// The client's own endpoint should track the client session.
const clientEpStats = clientSession.endpoint.stats;
strictEqual(clientEpStats.clientSessions, 1n);
ok(clientEpStats.bytesSent > 0n);
ok(clientEpStats.bytesReceived > 0n);

// --- toJSON / toString ---

const epJson = epStats.toJSON();
ok(epJson);
strictEqual(typeof epJson.bytesReceived, 'string');
strictEqual(typeof epJson.bytesSent, 'string');
strictEqual(typeof epJson.connected, 'boolean');

const ssJson = csStats.toJSON();
ok(ssJson);
strictEqual(typeof ssJson.handshakeCompletedAt, 'string');
strictEqual(typeof ssJson.messagesReceived, 'string');

const epStr = epStats.toString();
ok(typeof epStr === 'string');
ok(epStr.includes('bytesReceived'));

// Clean up.
await clientSession.close();

// After close, session closing timestamp should be set.
notStrictEqual(csStats.closingAt, 0n);

await endpoint.close();

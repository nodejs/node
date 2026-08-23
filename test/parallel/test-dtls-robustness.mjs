// Flags: --experimental-dtls --no-warnings --expose-internals

// Test: a listening DTLS server drops malformed datagrams without crashing,
// and still accepts a real client afterwards.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dgram from 'node:dgram';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

// The state and sessions views are not public API; they are reached here the
// way node:quic's tests reach theirs.
const {
  getDTLSEndpointSessions,
} = (await import('internal/dtls/dtls')).default;

const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();
const ca = fixtures.readKey('ca1-cert.pem').toString();

const server = listen(mustCall((session) => {
  session.onhandshake = mustCall();
}), { cert, key, port: 0, host: '127.0.0.1' });

const { port } = server.address;

// Fire datagrams that are not a ClientHello at the server. The empty one is
// legal UDP but can never carry a DTLS record; it must be dropped before it
// reaches the accept path or a session BIO.
const junk = [
  Buffer.from('this is not a DTLS ClientHello'),
  Buffer.alloc(0),
  Buffer.from([22]),
  Buffer.from([22, 254, 253, 0, 0]),
];

const raw = dgram.createSocket('udp4');
for (const datagram of junk) {
  await new Promise((resolve, reject) => {
    raw.send(datagram, port, '127.0.0.1',
             (err) => (err ? reject(err) : resolve()));
  });
}
await new Promise((resolve) => raw.close(resolve));

// A real client still completes a handshake against the same server.
const client = connect('127.0.0.1', port, {
  ca: [ca],
  rejectUnauthorized: false,
});

await client.opened;

// The junk was delivered and processed well before this handshake finished,
// so the counters are settled by now.

// Exactly one session, from the real client. None of the junk made one.
assert.strictEqual(getDTLSEndpointSessions(server).size, 1);
assert.strictEqual(server.stats.serverSessions, 1n);

// Three of the four junk datagrams were turned away by the structural screen
// before anything was allocated for them. The empty one was dropped earlier
// still, and the real ClientHello passed the screen rather than being counted
// by it.
assert.strictEqual(server.stats.serverRejectedCount, 3n);

await client.close();
await server.close();

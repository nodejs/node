// Flags: --experimental-dtls --no-warnings

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

// None of that should have produced a session.
assert.strictEqual(server.sessions.size, 0);
assert.strictEqual(server.stats.serverSessions, 0n);

// A real client still completes a handshake against the same server.
const client = connect('127.0.0.1', port, {
  ca: [ca],
  rejectUnauthorized: false,
});

await client.opened;

// ...and is counted, which also confirms the assertions above were not
// vacuously true.
assert.strictEqual(server.stats.serverSessions, 1n);

await client.close();
await server.close();

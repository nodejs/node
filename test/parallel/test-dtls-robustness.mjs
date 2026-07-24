// Flags: --experimental-dtls --no-warnings

// Test: a listening DTLS server drops a non-DTLS (junk) datagram without
// crashing, and still accepts a real client afterwards.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import dgram from 'node:dgram';
import * as fixtures from '../common/fixtures.mjs';

const { readKey } = fixtures;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

const cert = readKey('agent1-cert.pem').toString();
const key = readKey('agent1-key.pem').toString();
const ca = readKey('ca1-cert.pem').toString();

const server = listen(mustCall((session) => {
  session.onhandshake = mustCall();
}), { cert, key, port: 0, host: '127.0.0.1' });

const { port } = server.address;

// Fire a datagram that is not a ClientHello at the server.
const raw = dgram.createSocket('udp4');
await new Promise((resolve, reject) => {
  raw.send(Buffer.from('this is not a DTLS ClientHello'), port, '127.0.0.1',
           (err) => (err ? reject(err) : resolve()));
});
await new Promise((resolve) => raw.close(resolve));

// A real client still completes a handshake against the same server.
const client = connect('127.0.0.1', port, {
  ca: [ca],
  rejectUnauthorized: false,
});

await client.opened;

await client.close();
await server.close();

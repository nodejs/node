// Flags: --experimental-dtls --no-warnings

// Test: a client connect() whose handshake fails must tear down its internally
// owned endpoint, so the event loop can drain. Regression test for a failed
// connect leaking the endpoint (and hanging the process).

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { rejects } = assert;
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

// The client rejects the certificate mid-handshake, so this server session
// never opens; its opened rejection is handled internally by the library.
const server = listen(mustCall(), { cert, key, port: 0, host: '127.0.0.1' });

// A servername that does not match the certificate, under rejectUnauthorized,
// makes the client's handshake fail during verification.
const session = connect('127.0.0.1', server.address.port, {
  ca: [ca],
  rejectUnauthorized: true,
  servername: 'wrong.example.com',
});

await rejects(session.opened, /certificate verify failed/i);

// The failed connect must have closed its internally-owned endpoint. Without
// that, this await never settles and the test times out.
await session.endpoint.closed;

await server.close();

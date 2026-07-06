// Flags: --experimental-dtls --no-warnings

// Test: using the callback style (onerror) without awaiting opened/closed must
// not produce an unhandled promise rejection when the session errors.

import { hasCrypto, skip, mustCall, mustNotCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';

const { readKey } = fixtures;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

// Any unhandled rejection fails the test.
process.on('unhandledRejection', mustNotCall());

const { listen, connect } = await import('node:dtls');

const cert = readKey('agent1-cert.pem').toString();
const key = readKey('agent1-key.pem').toString();
const ca = readKey('ca1-cert.pem').toString();

const server = listen(mustCall(), {
  cert, key, port: 0, host: '127.0.0.1',
});

const errored = Promise.withResolvers();

// A verification failure makes connect() reject its opened promise. Handle the
// error only through the callback API and never touch opened/closed.
const session = connect('127.0.0.1', server.address.port, {
  ca: [ca],
  rejectUnauthorized: true,
  servername: 'wrong.example.com',
});
session.onerror = mustCall(() => errored.resolve());

await errored.promise;

// Give the rejected (but internally handled) opened promise a chance to be
// reported as unhandled, if it were, before the process exits.
await new Promise((resolve) => setImmediate(resolve));

await server.close();

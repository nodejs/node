// Flags: --experimental-dtls --no-warnings

// Test: the onkeylog callback delivers NSS-format key material during the
// handshake (useful for decrypting captures in Wireshark).

import { hasCrypto, skip, mustCall, mustCallAtLeast } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, match } = assert;
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

const gotKeylog = Promise.withResolvers();

const server = listen(mustCall(), {
  cert, key, port: 0, host: '127.0.0.1',
});

const client = connect('127.0.0.1', server.address.port, {
  ca: [ca],
  rejectUnauthorized: false,
});

// A keylog line is "<LABEL> <hex> <hex>" (e.g. "CLIENT_RANDOM ...").
client.onkeylog = mustCallAtLeast((line) => {
  strictEqual(typeof line, 'string');
  match(line, /^\S+ [0-9a-f]+ [0-9a-f]+$/i);
  gotKeylog.resolve();
});

await client.opened;
await gotKeylog.promise;

await client.close();
await server.close();

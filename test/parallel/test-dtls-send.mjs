// Flags: --experimental-dtls --no-warnings

// Test: DTLSSession.send() return values and the single-record size limit.

import { hasCrypto, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
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

// The largest application payload that fits in a single DTLS record (2^14).
const MAX_RECORD = 16384;

// Case 1: send() before the handshake returns -1; afterwards it returns the
// number of bytes written and the message is delivered whole.
{
  const received = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    session.onmessage = mustCall((data) => received.resolve(data.length));
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  strictEqual(client.send('before handshake'), -1);

  await client.opened;

  strictEqual(client.send(Buffer.alloc(MAX_RECORD, 0x61)), MAX_RECORD);
  strictEqual(await received.promise, MAX_RECORD);

  await client.close();
  await server.close();
}

// Case 2: a payload larger than a single record cannot be sent.
{
  const server = listen(mustCall((session) => {
    session.onmessage = mustNotCall();
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  await client.opened;
  strictEqual(client.send(Buffer.alloc(MAX_RECORD + 1, 0x61)), -1);

  await client.close();
  await server.close();
}

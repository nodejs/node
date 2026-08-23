// Flags: --experimental-dtls --no-warnings

// Test: DTLSSession.send() return values and the single-record size limit.
//
// send() used to return a bare -1 both for a payload too large for a DTLS
// record and for a send before the handshake finished. Nothing distinguished
// them, the value was undocumented, and `session.send(data)` as a statement
// discards it, so the data went missing silently. It now throws, which is what
// the same method already did for a destroyed session and a bad argument.

import { hasCrypto, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
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

// The largest application payload that fits in a single DTLS record (2^14).
const MAX_RECORD = 16384;

// Case 1: send() before the handshake is a state error; afterwards it returns
// the number of bytes written and the message is delivered whole.
{
  const received = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    session.onmessage = mustCall((data) => received.resolve(data.length));
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  assert.throws(
    () => client.send('before handshake'),
    { code: 'ERR_INVALID_STATE', message: /before the handshake completes/ });

  await client.opened;

  assert.strictEqual(client.send(Buffer.alloc(MAX_RECORD, 0x61)), MAX_RECORD);
  assert.strictEqual(await received.promise, MAX_RECORD);

  await client.close();
  await server.close();
}

// Case 2: a payload larger than a single record is refused, and the error
// names both the size given and the limit rather than just failing.
{
  const server = listen(mustCall((session) => {
    session.onmessage = mustNotCall();
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  await client.opened;

  for (const size of [MAX_RECORD + 1, 70000]) {
    assert.throws(
      () => client.send(Buffer.alloc(size, 0x61)),
      {
        code: 'ERR_OUT_OF_RANGE',
        message: new RegExp(`${size} bytes.*${MAX_RECORD} byte maximum`),
      },
      `${size} bytes should have been refused`);
  }

  await client.close();
  await server.close();
}

// Case 3: the record limit is what bounds send(), not the MTU. A record
// larger than the configured MTU is still sent and still arrives; IP
// fragments it. Getting this wrong would make send() reject valid payloads.
{
  const received = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    session.onmessage = mustCall((data) => received.resolve(data.length));
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
    mtu: 1200,
  });

  await client.opened;
  assert.strictEqual(client.send(Buffer.alloc(4000, 0x61)), 4000);
  assert.strictEqual(await received.promise, 4000);

  await client.close();
  await server.close();
}

// Case 4: a closed session reports that, and is not confused with the size
// or handshake errors above.
{
  const server = listen(mustCall(),
                        { cert, key, port: 0, host: '127.0.0.1' });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  await client.opened;
  await client.close();

  assert.throws(
    () => client.send('after close'),
    { code: 'ERR_INVALID_STATE' });

  await server.close();
}

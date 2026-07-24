// Flags: --experimental-dtls --no-warnings

// Test: a session is removed from its endpoint's session table when it closes,
// whether closed locally or by the peer. Regression test for closed sessions
// leaking in the table (which also blocks reuse of the peer's address).

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
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

// ---------------------------------------------------------------------------
// Case 1: the server closes its own session -> removed from the table.
{
  const gotSession = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    gotSession.resolve(session);
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  await client.opened;
  const session = await gotSession.promise;

  strictEqual(server.state.sessionCount, 1);
  await session.close();
  strictEqual(server.state.sessionCount, 0);

  await client.close();
  await server.close();
}

// ---------------------------------------------------------------------------
// Case 2: the client closes -> the server sees the peer close_notify and
// removes the session from the table.
{
  const gotSession = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    gotSession.resolve(session);
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  await client.opened;
  const session = await gotSession.promise;

  strictEqual(server.state.sessionCount, 1);
  await client.close();   // Sends close_notify to the server
  await session.closed;   // Server processes it, detaches, and emits its close
  strictEqual(server.state.sessionCount, 0);

  await server.close();
}

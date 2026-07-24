// Flags: --experimental-dtls --no-warnings

// Test: destroying a DTLS session synchronously from within a callback that is
// dispatched from the session's own I/O pump must not crash. The endpoint's
// session table holds the only strong reference to the session, so a reentrant
// destroy() removes it mid-pump; the implementation must keep the object alive
// until the pump unwinds (regression test for a use-after-free).

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
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

// ---------------------------------------------------------------------------
// Case 1: destroy the (server) session from inside onmessage. The datagram
// carrying the message drives receive -> pump -> onmessage -> destroy(), which
// frees the session's map entry while ClearOut() is still looping over ssl_.
{
  const destroyed = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    session.onmessage = mustCall(() => {
      session.destroy();
      destroyed.resolve();
    });
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  await client.opened;
  client.send('destroy me from onmessage');

  await destroyed.promise;

  await client.close();
  await server.close();
}

// ---------------------------------------------------------------------------
// Case 2: destroy the (server) session from inside onhandshake. Handshake
// completion is emitted from the middle of the pump (Cycle), so destroying
// there must not free the session before the pump finishes unwinding.
{
  const destroyed = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    session.onhandshake = mustCall(() => {
      session.destroy();
      destroyed.resolve();
    });
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const client = connect('127.0.0.1', server.address.port, {
    ca: [ca],
    rejectUnauthorized: false,
  });

  await destroyed.promise;

  await client.close();
  await server.close();
}

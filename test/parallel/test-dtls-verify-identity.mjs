// Flags: --experimental-dtls --no-warnings

// Test: the DTLS client verifies the server certificate's identity (hostname
// or IP address) against the expected name, not merely that the certificate
// chains to a trusted CA. Also exercises that SNI is actually carried in the
// ClientHello (it is applied before the handshake begins).

import { hasCrypto, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { match, rejects, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

// The agent1 certificate has CN=agent1 and no subjectAltName, so OpenSSL
// matches the identity "agent1" (via the subject CN) and rejects other names.
const cert = readKey('agent1-cert.pem').toString();
const key = readKey('agent1-key.pem').toString();
const ca = readKey('ca1-cert.pem').toString();

// ---------------------------------------------------------------------------
// Case 1: a matching servername with rejectUnauthorized succeeds, and the
// server observes the SNI the client sent (proving it is in the ClientHello).
{
  const sawServername = Promise.withResolvers();

  const server = listen(mustCall((session) => {
    session.onhandshake = mustCall(() => {
      sawServername.resolve(session.servername);
    });
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const { port } = server.address;

  const session = connect('127.0.0.1', port, {
    ca: [ca],
    rejectUnauthorized: true,
    servername: 'agent1',
  });

  const { protocol } = await session.opened;
  match(protocol, /DTLS/i);

  strictEqual(await sawServername.promise, 'agent1');

  await session.close();
  await server.close();
}

// ---------------------------------------------------------------------------
// Case 2: a servername that does not match the certificate is rejected during
// the handshake, even though the certificate chain is otherwise valid.
{
  const server = listen(mustCall((session) => {
    // The client aborts after verifying the certificate, so the server-side
    // handshake never completes.
    session.onhandshake = mustNotCall();
  }), { cert, key, port: 0, host: '127.0.0.1' });

  const { port } = server.address;

  const session = connect('127.0.0.1', port, {
    ca: [ca],
    rejectUnauthorized: true,
    servername: 'wrong.example.com',
  });

  await rejects(session.opened, /certificate verify failed/i);

  // Closing the session tears down the internally-owned client endpoint.
  await session.close();
  await server.close();
}

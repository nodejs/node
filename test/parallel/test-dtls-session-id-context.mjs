// Flags: --experimental-dtls --no-warnings

// Test: a server sets a session id context.
//
// OpenSSL refuses to resume a session whose id context differs from the one
// on the accepting SSL. Without it set, a session issued by one server can be
// resumed against another configured differently, which matters most when
// client certificates are involved. node:tls sets one; this did not.

import { hasCrypto, skip } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem');
const key = fixtures.readKey('agent1-key.pem');
const ca = fixtures.readKey('ca1-cert.pem');

// An explicit context is accepted and does not disturb the handshake.
{
  const endpoint = listen(() => {}, {
    cert, key, host: '127.0.0.1', port: 0,
    sessionIdContext: 'a'.repeat(32),
  });

  const client = connect('127.0.0.1', endpoint.address.port, {
    servername: 'agent1', ca: [ca],
  });
  await client.opened;
  assert.strictEqual(endpoint.stats.serverSessions, 1n);

  await client.close();
  await endpoint.close();
}

// The default is applied when none is given, and handshakes still work.
{
  const endpoint = listen(() => {}, {
    cert, key, host: '127.0.0.1', port: 0,
  });

  const client = connect('127.0.0.1', endpoint.address.port, {
    servername: 'agent1', ca: [ca],
  });
  await client.opened;
  assert.strictEqual(endpoint.stats.serverSessions, 1n);

  await client.close();
  await endpoint.close();
}

// SSL_CTX_set_session_id_context caps the context at 32 bytes and fails
// above it, which must surface as an error rather than being ignored.
{
  assert.throws(
    () => listen(() => {}, {
      cert, key, host: '127.0.0.1', port: 0,
      sessionIdContext: 'x'.repeat(33),
    }),
    { code: 'ERR_CRYPTO_OPERATION_FAILED' });
}

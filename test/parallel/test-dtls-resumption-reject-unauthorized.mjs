// Flags: --experimental-dtls --no-warnings

// Test: resumption does not launder an unverified peer past
// rejectUnauthorized.
//
// A resumed handshake sends no Certificate message, so OpenSSL runs no
// verification and the verify mode has nothing to abort on. What it does
// instead is restore the result recorded when the session was first
// established: SSL_set_session() copies verify_result straight onto the
// connection, and both it and the peer certificate round-trip through the DER
// blob. A session established under rejectUnauthorized: false therefore
// arrives carrying its original failure alongside a handshake that succeeded.
//
// Binding the blob to the identity it was authenticated for is not enough on
// its own: the identity here is the same host either way, and what changes is
// whether the caller asked for the peer to be verified at all. So the result
// is re-checked once the handshake completes, as node:tls checks
// verifyError() after a resumption.

import { hasCrypto, skip, mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { connect, listen } = await import('node:dtls');

const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();
const ca = fixtures.readKey('ca1-cert.pem').toString();

const serverOptions = { cert, key, host: '127.0.0.1', port: 0 };

const V12 = { minVersion: 'DTLSv1.2', maxVersion: 'DTLSv1.2' };
const V13 = { minVersion: 'DTLSv1.3', maxVersion: 'DTLSv1.3' };

// Connects, and returns the session with a ticket to resume it from. A DTLS
// 1.3 server sends its tickets after the handshake, so there the first is
// taken from onsessionticket rather than read from session.session.
async function ticketed(port, versions, options) {
  const session = connect('127.0.0.1', port, { ...options, ...versions });
  const issued = Promise.withResolvers();
  session.onsessionticket = (ticket) => issued.resolve(ticket);
  await session.opened;
  const ticket = versions === V12 ? session.session : await issued.promise;
  return { session, ticket };
}

// A ticket earned without verification cannot be spent with it. The chain
// does not verify here -- no `ca` is supplied, so only the system-default CAs
// are trusted -- and rejectUnauthorized: false is what let the first
// connection through.
for (const versions of [V12, V13]) {
  const server = listen(mustCall(2), { ...serverOptions, ...versions });

  const { session: first, ticket } = await ticketed(
    server.address.port, versions, { rejectUnauthorized: false });

  // The failure that the blob is about to carry with it.
  assert.strictEqual(first.authorized, false);
  assert.ok(first.authorizationError);
  assert.ok(Buffer.isBuffer(ticket));
  await first.close();

  // Same host, so the identity binding permits the blob; the difference is
  // that this caller asked for a verified peer.
  const second = connect('127.0.0.1', server.address.port, {
    rejectUnauthorized: true, session: ticket, ...versions,
  });

  await assert.rejects(second.opened, {
    code: 'ERR_INVALID_STATE',
    message: /Peer certificate verification failed/,
  });

  await second.endpoint.closed;
  await server.close();
}

// The same check must not get in the way of a resumption that was verified
// for real. With the CA trusted and the identity matching the certificate,
// the first handshake verifies, the recorded result is X509_V_OK, and the
// resumed handshake inherits it.
for (const versions of [V12, V13]) {
  const server = listen(mustCall(2), { ...serverOptions, ...versions });

  const { session: first, ticket } = await ticketed(
    server.address.port, versions,
    { ca, servername: 'agent1', rejectUnauthorized: true });
  assert.strictEqual(first.authorized, true);
  assert.strictEqual(first.authorizationError, undefined);
  await first.close();

  const second = connect('127.0.0.1', server.address.port, {
    ca, servername: 'agent1', rejectUnauthorized: true, session: ticket,
    ...versions,
  });
  await second.opened;

  assert.strictEqual(second.reused, true);
  assert.strictEqual(second.authorized, true);
  assert.strictEqual(second.authorizationError, undefined);

  await second.close();
  await server.close();
}

// A caller that did not ask for verification still gets the connection, and
// still gets told the peer never verified. Resumption does not change that
// either way.
for (const versions of [V12, V13]) {
  const server = listen(mustCall(2), { ...serverOptions, ...versions });

  const { session: first, ticket } = await ticketed(
    server.address.port, versions, { rejectUnauthorized: false });
  const firstError = first.authorizationError;
  await first.close();

  const second = connect('127.0.0.1', server.address.port, {
    rejectUnauthorized: false, session: ticket, ...versions,
  });
  await second.opened;

  assert.strictEqual(second.reused, true);
  assert.strictEqual(second.authorized, false);
  // Restored from the session rather than verified again.
  assert.strictEqual(second.authorizationError, firstError);

  await second.close();
  await server.close();
}

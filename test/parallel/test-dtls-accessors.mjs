// Flags: --experimental-dtls --no-warnings --expose-internals

// Test: DTLSEndpoint/DTLSSession state fields and callback accessors reflect
// what is set and the connection lifecycle.

import {
  hasCrypto, skip, mustCall, mustNotCall, mustCallAtLeast,
} from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasCrypto) {
  skip('missing crypto');
}

if (!process.features.dtls) {
  skip('DTLS is not enabled');
}

const { listen, connect } = await import('node:dtls');

// The state view is not public API; reached the way node:quic's tests do.
const {
  getDTLSEndpointState,
  getDTLSSessionState,
} = (await import('internal/dtls/dtls')).default;
const { kOwnsEndpoint } = (await import('internal/dtls/symbols')).default;

const cert = fixtures.readKey('agent1-cert.pem').toString();
const key = fixtures.readKey('agent1-key.pem').toString();
const ca = fixtures.readKey('ca1-cert.pem').toString();

const gotServerSession = Promise.withResolvers();

const server = listen(mustCall((session) => {
  gotServerSession.resolve(session);
}), { cert, key, port: 0, host: '127.0.0.1' });

// --- Endpoint state after listen(): bound and listening. ---
const es = getDTLSEndpointState(server);
assert.strictEqual(es.bound, true);
assert.strictEqual(es.listening, true);
assert.strictEqual(es.closing, false);
assert.strictEqual(es.destroyed, false);
assert.strictEqual(es.sessionCount, 0);

// The busy property is settable via the endpoint and reflected in the state view.
assert.strictEqual(server.busy, false);
assert.strictEqual(es.busy, false);
server.busy = true;
assert.strictEqual(server.busy, true);
assert.strictEqual(es.busy, true);
server.busy = false;
assert.strictEqual(es.busy, false);

// --- Endpoint onerror accessor. ---
assert.strictEqual(server.onerror, undefined);
const onEndpointError = mustNotCall();
server.onerror = onEndpointError;
assert.strictEqual(server.onerror, onEndpointError);

const client = connect('127.0.0.1', server.address.port, {
  ca: [ca],
  rejectUnauthorized: false,
});

// --- Session state during the handshake. ---
const cs = getDTLSSessionState(client);
assert.strictEqual(cs.handshaking, true);
assert.strictEqual(cs.open, false);
assert.strictEqual(cs.closing, false);
assert.strictEqual(cs.destroyed, false);
assert.strictEqual(cs.hasMessageListener, false);

// --- Session callback accessors: unset, then set. ---
assert.strictEqual(client.onmessage, undefined);
assert.strictEqual(client.onerror, undefined);
assert.strictEqual(client.onhandshake, undefined);
assert.strictEqual(client.onkeylog, undefined);
// A connect() session owns its internal endpoint, which is recorded on a
// symbol rather than a property: it decides whether closing the session takes
// the endpoint with it, and a server session told the same thing would take
// down the listener.
assert.strictEqual(client[kOwnsEndpoint], true);
assert.strictEqual(client.ownsEndpoint, undefined);

client.onmessage = mustNotCall();
assert.strictEqual(typeof client.onmessage, 'function');
// Attaching a message listener flips the shared flag.
assert.strictEqual(cs.hasMessageListener, true);

client.onerror = mustNotCall();
assert.strictEqual(typeof client.onerror, 'function');

client.onhandshake = mustCall();
assert.strictEqual(typeof client.onhandshake, 'function');

client.onkeylog = mustCallAtLeast();
assert.strictEqual(typeof client.onkeylog, 'function');

await client.opened;

// --- Session state after the handshake completes. ---
assert.strictEqual(cs.handshaking, false);
assert.strictEqual(cs.open, true);

const serverSession = await gotServerSession.promise;
await serverSession.opened;
assert.strictEqual(es.sessionCount, 1);

await client.close();
await server.close();

// ownsEndpoint is not public. It says whether closing a session should take
// its endpoint with it, which is true only for the single-session endpoint
// connect() creates. Exposed as a settable property, it let a server session
// be told it owned the listener: closing that one session then tore down the
// endpoint and every other session on it.
{
  const arrived = Promise.withResolvers();
  let serverSession;

  const server = listen((session) => {
    serverSession ??= session;
    arrived.resolve();
  }, { cert, key, host: '127.0.0.1', port: 0 });
  const port = server.address.port;

  const first = connect('127.0.0.1', port, { rejectUnauthorized: false });
  await first.opened;
  await arrived.promise;

  assert.strictEqual(serverSession.ownsEndpoint, undefined);
  assert.strictEqual(
    Object.getOwnPropertyDescriptor(
      Object.getPrototypeOf(serverSession), 'ownsEndpoint'),
    undefined);

  // Assigning the name creates an ordinary own property and nothing more.
  serverSession.ownsEndpoint = true;
  await serverSession.close();

  // The endpoint is still listening and still serving.
  assert.notStrictEqual(server.address, undefined);
  const second = connect('127.0.0.1', port, {
    rejectUnauthorized: false,
    handshakeTimeout: 5000,
  });
  await second.opened;

  await second.close();
  await first.close();
  await server.close();
}

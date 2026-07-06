// Flags: --experimental-dtls --no-warnings

// Test: DTLSEndpoint/DTLSSession state fields and callback accessors reflect
// what is set and the connection lifecycle.

import {
  hasCrypto, skip, mustCall, mustNotCall, mustCallAtLeast,
} from '../common/index.mjs';
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

const gotServerSession = Promise.withResolvers();

const server = listen(mustCall((session) => {
  gotServerSession.resolve(session);
}), { cert, key, port: 0, host: '127.0.0.1' });

// --- Endpoint state after listen(): bound and listening. ---
const es = server.state;
strictEqual(es.bound, true);
strictEqual(es.listening, true);
strictEqual(es.closing, false);
strictEqual(es.destroyed, false);
strictEqual(es.sessionCount, 0);

// The busy property is settable via the endpoint and reflected in the state view.
strictEqual(server.busy, false);
strictEqual(es.busy, false);
server.busy = true;
strictEqual(server.busy, true);
strictEqual(es.busy, true);
server.busy = false;
strictEqual(es.busy, false);

// --- Endpoint onerror accessor. ---
strictEqual(server.onerror, undefined);
const onEndpointError = mustNotCall();
server.onerror = onEndpointError;
strictEqual(server.onerror, onEndpointError);

const client = connect('127.0.0.1', server.address.port, {
  ca: [ca],
  rejectUnauthorized: false,
});

// --- Session state during the handshake. ---
const cs = client.state;
strictEqual(cs.handshaking, true);
strictEqual(cs.open, false);
strictEqual(cs.closing, false);
strictEqual(cs.destroyed, false);
strictEqual(cs.hasMessageListener, false);

// --- Session callback accessors: unset, then set. ---
strictEqual(client.onmessage, undefined);
strictEqual(client.onerror, undefined);
strictEqual(client.onhandshake, undefined);
strictEqual(client.onkeylog, undefined);
// A connect() session owns its internal endpoint.
strictEqual(client.ownsEndpoint, true);

client.onmessage = mustNotCall();
strictEqual(typeof client.onmessage, 'function');
// Attaching a message listener flips the shared flag.
strictEqual(cs.hasMessageListener, true);

client.onerror = mustNotCall();
strictEqual(typeof client.onerror, 'function');

client.onhandshake = mustCall();
strictEqual(typeof client.onhandshake, 'function');

client.onkeylog = mustCallAtLeast();
strictEqual(typeof client.onkeylog, 'function');

await client.opened;

// --- Session state after the handshake completes. ---
strictEqual(cs.handshaking, false);
strictEqual(cs.open, true);

const serverSession = await gotServerSession.promise;
await serverSession.opened;
strictEqual(es.sessionCount, 1);

await client.close();
await server.close();

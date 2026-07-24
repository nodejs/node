// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: diagnostics_channel events.
// quic.endpoint.created fires when endpoint is created.
// quic.endpoint.listen fires when endpoint starts listening.
// quic.endpoint.closing fires when endpoint begins closing.
// quic.endpoint.closed fires when endpoint finishes closing.
// quic.session.created.client fires for client sessions.
// quic.session.created.server fires for server sessions.
// quic.session.closing fires when session begins closing.
// quic.session.closed fires when session finishes closing.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const events = [];

// endpoint.created fires for both server and client endpoints.
dc.subscribe('quic.endpoint.created', mustCall((msg) => {
  events.push('endpoint.created');
  assert.ok(msg.endpoint);
}, 2));

// endpoint.listen fires once (server only).
dc.subscribe('quic.endpoint.listen', mustCall((msg) => {
  events.push('endpoint.listen');
  assert.ok(msg.endpoint);
}));

// endpoint.closing fires once (server endpoint closes).
dc.subscribe('quic.endpoint.closing', mustCall((msg) => {
  events.push('endpoint.closing');
  assert.ok(msg.endpoint);
}));

// endpoint.closed fires once (server endpoint closed).
dc.subscribe('quic.endpoint.closed', mustCall((msg) => {
  events.push('endpoint.closed');
  assert.ok(msg.endpoint);
  assert.ok(msg.stats, 'endpoint.closed should include stats');
}));

// endpoint.connect fires before a client session is created.
dc.subscribe('quic.endpoint.connect', mustCall((msg) => {
  events.push('endpoint.connect');
  assert.ok(msg.endpoint);
  assert.ok(msg.address);
  assert.ok(msg.options);
}));

// session.created.client fires for the client session.
dc.subscribe('quic.session.created.client', mustCall((msg) => {
  events.push('session.created.client');
  assert.ok(msg.session);
}));

// session.created.server fires for the server session.
dc.subscribe('quic.session.created.server', mustCall((msg) => {
  events.push('session.created.server');
  assert.ok(msg.session);
  assert.ok(msg.address, 'server session should include remote address');
}));

// session.closing fires when session.close() is called.
// Only fires for sessions where close() is explicitly called (the server).
// The client session closes via CONNECTION_CLOSE without going through close().
dc.subscribe('quic.session.closing', mustCall((msg) => {
  events.push('session.closing');
  assert.ok(msg.session);
}));

// session.closed fires when session is fully closed.
dc.subscribe('quic.session.closed', mustCall((msg) => {
  events.push('session.closed');
  assert.ok(msg.session);
  assert.ok(msg.stats, 'session.closed should include stats');
}, 2));

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  serverSession.close();
  await serverSession.closed;
}));

const clientSession = await connect(serverEndpoint.address);
await Promise.all([clientSession.opened, clientSession.closed]);

await serverEndpoint.close();

// Verify key events occurred.
assert.ok(events.includes('endpoint.created'), 'missing endpoint.created');
assert.ok(events.includes('endpoint.listen'), 'missing endpoint.listen');
assert.ok(events.includes('endpoint.connect'), 'missing endpoint.connect');
assert.ok(events.includes('session.created.client'), 'missing session.created.client');
assert.ok(events.includes('session.created.server'), 'missing session.created.server');
assert.ok(events.includes('endpoint.closing'), 'missing endpoint.closing');
assert.ok(events.includes('endpoint.closed'), 'missing endpoint.closed');

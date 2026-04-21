// Flags: --experimental-quic --no-warnings

// Test: diagnostics_channel token/ticket events.
// quic.session.ticket fires when session ticket received.
// quic.session.new.token fires when NEW_TOKEN received.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

const { ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const allDone = Promise.withResolvers();
let ticketFired = false;
let tokenFired = false;

function checkDone() {
  if (ticketFired && tokenFired) allDone.resolve();
}

// quic.session.ticket fires when session ticket received.
dc.subscribe('quic.session.ticket', mustCall((msg) => {
  ok(msg.session);
  ok(msg.ticket);
  ticketFired = true;
  checkDone();
}));

// quic.session.new.token fires when NEW_TOKEN received.
dc.subscribe('quic.session.new.token', mustCall((msg) => {
  ok(msg.session);
  ok(msg.token);
  ok(msg.address);
  tokenFired = true;
  checkDone();
}));

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}));

const clientSession = await connect(serverEndpoint.address, {
  onsessionticket: mustCall((ticket) => { ok(ticket); }),
  onnewtoken: mustCall((token) => { ok(token); }),
});
await Promise.all([clientSession.opened, allDone.promise]);
await clientSession.close();
await serverEndpoint.close();

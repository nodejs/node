// Flags: --experimental-quic --no-warnings

// Test: 0-RTT rejected when server settings change.
// The client has a session ticket from a server with generous
// transport params. The server restarts with reduced params
// (smaller initialMaxStreamsBidi). The 0-RTT should be rejected
// because the stored transport params are more permissive than
// the current ones. The connection falls back to 1-RTT.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';
import * as fixtures from '../common/fixtures.mjs';

const { ok, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey, randomBytes } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const sni = { '*': { keys: [key], certs: [cert] } };
const alpn = ['quic-test'];
const tokenSecret = randomBytes(16);

// quic.session.early.rejected fires when 0-RTT is rejected.
dc.subscribe('quic.session.early.rejected', mustCall((msg) => {
  ok(msg.session, 'early.rejected should include session');
}));

let savedTicket;
let savedToken;
const gotTicket = Promise.withResolvers();
const gotToken = Promise.withResolvers();

// First server: generous transport params.
const ep1 = await listen(async (serverSession) => {
  await serverSession.closed;
}, {
  sni,
  alpn,
  endpoint: { tokenSecret },
  transportParams: {
    initialMaxStreamsBidi: 100,
    initialMaxData: 1048576,
  },
});

const cs1 = await connect(ep1.address, {
  alpn: 'quic-test',
  onsessionticket: mustCall((ticket) => {
    savedTicket = ticket;
    gotTicket.resolve();
  }),
  onnewtoken: mustCall((token) => {
    savedToken = token;
    gotToken.resolve();
  }),
});
await Promise.all([cs1.opened, gotTicket.promise, gotToken.promise]);
await cs1.close();
await ep1.close();

// Second server: reduced transport params.
// initialMaxStreamsBidi reduced from 100 to 10.
const serverStreamSeen = Promise.withResolvers();
const ep2 = await listen((serverSession) => {
  serverSession.onstream = (stream) => {
    // The stream may be destroyed by EarlyDataRejected before
    // we can process it. Just record that we saw it.
    serverStreamSeen.resolve(true);
  };
}, {
  sni,
  alpn,
  endpoint: { tokenSecret },
  transportParams: {
    initialMaxStreamsBidi: 10,
    initialMaxData: 1048576,
  },
  onerror(err) { ok(err); },
});

const cs2 = await connect(ep2.address, {
  alpn: 'quic-test',
  sessionTicket: savedTicket,
  token: savedToken,
  onerror(err) { ok(err); },
  onearlyrejected() {},
});

// Trigger the deferred handshake.
const encoder = new TextEncoder();
await cs2.createBidirectionalStream({
  body: encoder.encode('test'),
});

const info2 = await cs2.opened;
// 0-RTT was attempted but rejected due to changed transport params.
strictEqual(info2.earlyDataAttempted, true);
strictEqual(info2.earlyDataAccepted, false);

// The 0-RTT stream may have been destroyed by EarlyDataRejected.
// Close from the client side.
await cs2.close();
await ep2.close();

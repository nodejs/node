// Flags: --experimental-quic --no-warnings

// Test: Server rejects 0-RTT when enableEarlyData: false.
// The client has a valid session ticket and token, and attempts 0-RTT.
// The server has enableEarlyData: false, so it rejects the early data.
// The connection should still succeed (fallback to 1-RTT), and
// earlyDataAccepted should be false.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual } = assert;
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

// Use the same tokenSecret for both servers so the token is valid.
const tokenSecret = randomBytes(16);

let savedTicket;
let savedToken;
const gotTicket = Promise.withResolvers();
const gotToken = Promise.withResolvers();

// First server: enableEarlyData: true (default) to generate a valid ticket.
const serverEndpoint1 = await listen((serverSession) => {
  serverSession.onstream = async (stream) => {
    for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
  };
}, {
  sni,
  alpn,
  endpoint: { tokenSecret },
});

const cs1 = await connect(serverEndpoint1.address, {
  alpn: 'quic-test',
  onsessionticket(ticket) {
    savedTicket = ticket;
    gotTicket.resolve();
  },
  onnewtoken(token) {
    savedToken = token;
    gotToken.resolve();
  },
});
await Promise.all([cs1.opened, gotTicket.promise, gotToken.promise]);
await cs1.close();
await serverEndpoint1.close();

// Second server: enableEarlyData: false — rejects 0-RTT.
const serverEndpoint2 = await listen(async (serverSession) => {
  await serverSession.opened;
  serverSession.close();
  await serverSession.closed;
}, {
  sni,
  alpn,
  enableEarlyData: false,
  endpoint: { tokenSecret },
  onerror: mustNotCall(),
});

const cs2 = await connect(serverEndpoint2.address, {
  alpn: 'quic-test',
  sessionTicket: savedTicket,
  token: savedToken,
});

// The deferred handshake needs a send to trigger. Use sendDatagram
// since it's simpler than a stream for this test.
await cs2.sendDatagram(new Uint8Array([1]));

const info2 = await cs2.opened;
strictEqual(info2.earlyDataAttempted, true);
strictEqual(info2.earlyDataAccepted, false);

await cs2.closed;
await serverEndpoint2.close();

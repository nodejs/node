// Flags: --experimental-quic --no-warnings

// Test: 0-RTT not attempted when client sets enableEarlyData: false
// Even with a valid session ticket and token, the client should not
// attempt 0-RTT when enableEarlyData is false. The opened info
// should show earlyDataAttempted: false.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

let savedTicket;
let savedToken;
const gotTicket = Promise.withResolvers();
const gotToken = Promise.withResolvers();

const serverEndpoint = await listen((serverSession) => {
  serverSession.onstream = async (stream) => {
    for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
  };
});

// First connection: get ticket and token.
const cs1 = await connect(serverEndpoint.address, {
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

// Second connection: provide ticket and token but disable early data.
const cs2 = await connect(serverEndpoint.address, {
  sessionTicket: savedTicket,
  token: savedToken,
  enableEarlyData: false,
});

const info2 = await cs2.opened;
// 0-RTT should NOT be attempted.
strictEqual(info2.earlyDataAttempted, false);
strictEqual(info2.earlyDataAccepted, false);

await cs2.close();
await serverEndpoint.close();

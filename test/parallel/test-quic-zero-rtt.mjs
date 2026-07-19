// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: 0-RTT session resumption.
// First connection receives a session ticket and NEW_TOKEN.
// Second connection uses both the session ticket and token.
//          The token skips address validation (Retry), the session
//          ticket enables 0-RTT encryption. The client sends data
//          BEFORE the handshake completes (true 0-RTT). The server's
//          onstream fires and the stream's early flag is true.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();

let savedTicket;
let savedToken;
const gotTicket = Promise.withResolvers();
const gotToken = Promise.withResolvers();

let firstStreamEarly;
let secondStreamEarly;
const secondStreamDone = Promise.withResolvers();

let serverSessionCount = 0;
const serverEndpoint = await listen((serverSession) => {
  const sessionNum = ++serverSessionCount;
  serverSession.onstream = async (stream) => {
    const data = await bytes(stream);
    ok(data.byteLength > 0);

    if (sessionNum === 1) {
      firstStreamEarly = stream.early;
    } else {
      secondStreamEarly = stream.early;
      secondStreamDone.resolve();
    }

    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
  };
});

// --- ZRTT-01: First connection — receive the session ticket and token ---
const cs1 = await connect(serverEndpoint.address, {
  onsessionticket: mustCall((ticket) => {
    ok(Buffer.isBuffer(ticket));
    ok(ticket.length > 0);
    savedTicket = ticket;
    gotTicket.resolve();
  }, 2),
  onnewtoken: mustCall((token) => {
    ok(Buffer.isBuffer(token));
    ok(token.length > 0);
    savedToken = token;
    gotToken.resolve();
  }),
});

const info1 = await cs1.opened;
strictEqual(info1.earlyDataAttempted, false);
strictEqual(info1.earlyDataAccepted, false);

await Promise.all([gotTicket.promise, gotToken.promise]);

// Send data to verify the connection works.
const s1 = await cs1.createBidirectionalStream({
  body: encoder.encode('first'),
});
for await (const _ of s1) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([s1.closed, cs1.closed]);

// --- ZRTT-02: Second connection — 0-RTT with ticket + token ---
// The token skips Retry (address validation), the session ticket
// enables 0-RTT encryption. With the deferred handshake, the
// stream data is included in the first flight as 0-RTT.
const cs2 = await connect(serverEndpoint.address, {
  sessionTicket: savedTicket,
  token: savedToken,
});

// Send data BEFORE the handshake completes — true 0-RTT.
const s2 = await cs2.createBidirectionalStream({
  body: encoder.encode('early data'),
});

// Now wait for handshake completion.
const info2 = await cs2.opened;
strictEqual(info2.earlyDataAttempted, true);
strictEqual(info2.earlyDataAccepted, true);

for await (const _ of s2) { /* drain */ } // eslint-disable-line no-unused-vars
await s2.closed;

// Verify the server saw the early data flag.
await secondStreamDone.promise;
strictEqual(firstStreamEarly, false);
strictEqual(secondStreamEarly, true);

await cs2.closed;
await serverEndpoint.close();

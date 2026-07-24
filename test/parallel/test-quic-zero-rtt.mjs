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
const serverEndpoint = await listen(mustCall((serverSession) => {
  const sessionNum = ++serverSessionCount;
  serverSession.onstream = mustCall((stream) =>
    bytes.then((data) => {
      assert.ok(data.byteLength > 0);

      if (sessionNum === 1) {
        firstStreamEarly = stream.early;
      } else {
        secondStreamEarly = stream.early;
        secondStreamDone.resolve();
      }

      stream.writer.endSync();
      return stream.closed;

    }).then(mustCall(() => serverSession.close()))
  );
}));

// --- ZRTT-01: First connection — receive the session ticket and token ---
const cs1 = await connect(serverEndpoint.address, {
  onsessionticket: mustCall((ticket) => {
    assert.ok(Buffer.isBuffer(ticket));
    assert.ok(ticket.length > 0);
    savedTicket = ticket;
    gotTicket.resolve();
  }, 2),
  onnewtoken: mustCall((token) => {
    assert.ok(Buffer.isBuffer(token));
    assert.ok(token.length > 0);
    savedToken = token;
    gotToken.resolve();
  }),
});

const info1 = await cs1.opened;
assert.strictEqual(info1.earlyDataAttempted, false);
assert.strictEqual(info1.earlyDataAccepted, false);

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
const s2 = await cs2.createBidirectionalStream();
await s2.writer.write(encoder.encode('early data'));

// Now wait for handshake completion.
const info2 = await cs2.opened;
assert.strictEqual(info2.earlyDataAttempted, true);
assert.strictEqual(info2.earlyDataAccepted, true);

await s2.writer.end();
for await (const _ of s2) { /* drain */ } // eslint-disable-line no-unused-vars
await s2.closed;

// Verify the server saw the early data flag.
await secondStreamDone.promise;
assert.strictEqual(firstStreamEarly, false);
assert.strictEqual(secondStreamEarly, true);

await cs2.closed;
await serverEndpoint.close();

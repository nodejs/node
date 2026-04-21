// Flags: --experimental-quic --no-warnings

// Test: 0-RTT with datagrams.
// The client sends a datagram as 0-RTT data in the first flight.
// The server receives it with the early flag set on the ondatagram
// callback. The datagram's early parameter should be true.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual, deepStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

let savedTicket;
let savedToken;
const gotTicket = Promise.withResolvers();
const gotToken = Promise.withResolvers();

let earlyDatagramReceived = false;
let receivedDatagramData;
const serverGotDatagram = Promise.withResolvers();

let serverSessionCount = 0;
const serverEndpoint = await listen((serverSession) => {
  const sessionNum = ++serverSessionCount;
  if (sessionNum === 2) {
    serverSession.ondatagram = (data, early) => {
      receivedDatagramData = Buffer.from(data);
      earlyDatagramReceived = early;
      serverGotDatagram.resolve();
    };
  }
}, {
  transportParams: { maxDatagramFrameSize: 1200 },
});

// --- First connection: receive session ticket and token ---
const cs1 = await connect(serverEndpoint.address, {
  transportParams: { maxDatagramFrameSize: 1200 },
  onsessionticket: mustCall((ticket) => {
    ok(Buffer.isBuffer(ticket));
    savedTicket = ticket;
    gotTicket.resolve();
  }),
  onnewtoken: mustCall((token) => {
    ok(Buffer.isBuffer(token));
    savedToken = token;
    gotToken.resolve();
  }),
});

await cs1.opened;
await Promise.all([gotTicket.promise, gotToken.promise]);
await cs1.close();

// --- Second connection: send datagram as 0-RTT ---
const cs2 = await connect(serverEndpoint.address, {
  transportParams: { maxDatagramFrameSize: 1200 },
  sessionTicket: savedTicket,
  token: savedToken,
});

// Send datagram BEFORE the handshake completes — true 0-RTT.
await cs2.sendDatagram(new Uint8Array([0xCA, 0xFE]));

const info2 = await cs2.opened;
strictEqual(info2.earlyDataAttempted, true);
strictEqual(info2.earlyDataAccepted, true);

// Verify the server received the datagram as early data.
await serverGotDatagram.promise;
deepStrictEqual(receivedDatagramData, Buffer.from([0xCA, 0xFE]));
strictEqual(earlyDatagramReceived, true);

await cs2.close();
await serverEndpoint.close();

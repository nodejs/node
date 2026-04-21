// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 0-RTT session resumption with session ticket app data.
// Session ticket includes HTTP/3 settings
// H3 + 0-RTT: Client sends H3 request in 0-RTT flight
// Uses a single server endpoint for both connections so the TLS
// session ticket encryption key is shared.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const encoder = new TextEncoder();
const decoder = new TextDecoder();

let savedTicket;
let savedToken;
const gotTicket = Promise.withResolvers();
const gotToken = Promise.withResolvers();

let serverSessionCount = 0;
const secondDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((ss) => {
  const num = ++serverSessionCount;
  ss.onstream = mustCall(async (stream) => {
    if (num === 2) {
      // Resolve with the stream so we can check stream.early after
      // data has been received (the early flag is set after
      // nghttp3 processes the 0-RTT headers, not at stream creation).
      secondDone.resolve(stream);
    }
    await stream.closed;
    ss.close();
  });
}, 2), {
  sni: { '*': { keys: [key], certs: [cert] } },
  onheaders: mustCall(function(headers) {
    this.sendHeaders({ ':status': '200' });
    this.writer.writeSync(encoder.encode(headers[':path']));
    this.writer.endSync();
  }, 2),
});

// --- First connection: establish H3 session, receive ticket ---
const cs1 = await connect(serverEndpoint.address, {
  servername: 'localhost',
  onsessionticket: mustCall(function(ticket) {
    ok(Buffer.isBuffer(ticket));
    ok(ticket.length > 0);
    savedTicket = ticket;
    gotTicket.resolve();
  }, 2),
  onnewtoken: mustCall(function(token) {
    ok(Buffer.isBuffer(token));
    savedToken = token;
    gotToken.resolve();
  }),
});

const info1 = await cs1.opened;
strictEqual(info1.earlyDataAttempted, false);
strictEqual(info1.earlyDataAccepted, false);

await Promise.all([gotTicket.promise, gotToken.promise]);

const s1 = await cs1.createBidirectionalStream({
  headers: {
    ':method': 'GET',
    ':path': '/first',
    ':scheme': 'https',
    ':authority': 'localhost',
  },
  onheaders: mustCall(function(headers) {
    strictEqual(headers[':status'], '200');
  }),
});
const body1 = await bytes(s1);
strictEqual(decoder.decode(body1), '/first');
await Promise.all([s1.closed, cs1.closed]);

// Session ticket should have been received.
ok(savedTicket);
ok(savedToken);

// --- Second connection: 0-RTT with H3 ---
const cs2 = await connect(serverEndpoint.address, {
  servername: 'localhost',
  sessionTicket: savedTicket,
  token: savedToken,
});

// Send H3 request BEFORE handshake completes — true 0-RTT.
const s2 = await cs2.createBidirectionalStream({
  headers: {
    ':method': 'GET',
    ':path': '/early',
    ':scheme': 'https',
    ':authority': 'localhost',
  },
  onheaders: mustCall(function(headers) {
    strictEqual(headers[':status'], '200');
  }),
});

const info2 = await cs2.opened;
strictEqual(info2.earlyDataAttempted, true);
strictEqual(info2.earlyDataAccepted, true);

const body2 = await bytes(s2);
strictEqual(decoder.decode(body2), '/early');
await s2.closed;

const earlyStream = await secondDone.promise;
strictEqual(earlyStream.early, true);

await cs2.closed;
await serverEndpoint.close();

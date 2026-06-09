// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: the generic `appTicketData` server option. Opaque application data
// configured on the server is embedded in issued session tickets and used to
// byte-match-validate 0-RTT on resume. Covers:
//   - Accept: resuming against an unchanged `appTicketData` accepts 0-RTT (the
//     embed + parse + byte-match round-trip works and does not break resumption).
//   - Reject: resuming against a different `appTicketData` refuses 0-RTT (the
//     byte-match fails) and falls back to a full 1-RTT handshake. Two endpoints
//     sharing the cert + tokenSecret are used so the ticket still decrypts and
//     the token validates; only the appTicketData differs.
//   - Argument validation: `appTicketData` must be an ArrayBufferView.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { randomBytes } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const appTicketData = new Uint8Array([1, 2, 3, 4, 5]);

// --- Accept path: appTicketData unchanged → 0-RTT accepted ---
const serverEndpoint = await listen((serverSession) => {
  serverSession.onstream = async (stream) => {
    try {
      await bytes(stream);
      stream.writer.endSync();
      await stream.closed;
    } catch { /* connection winding down */ }
  };
}, { appTicketData });

let savedTicket;
let savedToken;
const gotTicket = Promise.withResolvers();
const gotToken = Promise.withResolvers();

const cs1 = await connect(serverEndpoint.address, {
  onsessionticket: mustCall((ticket) => {
    savedTicket ??= ticket;
    gotTicket.resolve();
  }, 2),
  onnewtoken: mustCall((token) => {
    savedToken ??= token;
    gotToken.resolve();
  }),
});
await cs1.opened;
await Promise.all([gotTicket.promise, gotToken.promise]);

const s1 = await cs1.createBidirectionalStream({ body: encoder.encode('first') });
for await (const _ of s1) { /* drain */ } // eslint-disable-line no-unused-vars
await s1.closed;
await cs1.close();

// Resume against the same endpoint (same appTicketData) — 0-RTT accepted.
const cs2 = await connect(serverEndpoint.address, {
  sessionTicket: savedTicket,
  token: savedToken,
});
const s2 = await cs2.createBidirectionalStream({ body: encoder.encode('early') });
s2.closed.catch(() => {});
const info2 = await cs2.opened;
strictEqual(info2.earlyDataAttempted, true);
strictEqual(info2.earlyDataAccepted, true);

for await (const _ of s2) { /* drain */ } // eslint-disable-line no-unused-vars
await cs2.close();
await serverEndpoint.close();

// --- Reject path: a different appTicketData → 0-RTT rejected ---
{
  const tokenSecret = randomBytes(16);
  const handler = (session) => {
    session.onstream = async (stream) => {
      try {
        await bytes(stream);
        stream.writer.endSync();
        await stream.closed;
      } catch { /* connection winding down */ }
    };
  };

  const ep1 = await listen(handler, {
    appTicketData: new Uint8Array([1, 2, 3]),
    endpoint: { tokenSecret },
  });
  let ticket;
  let token;
  const haveTicket = Promise.withResolvers();
  const haveToken = Promise.withResolvers();
  const c1 = await connect(ep1.address, {
    onsessionticket: mustCall((t) => { ticket ??= t; haveTicket.resolve(); }, 2),
    onnewtoken: mustCall((t) => { token ??= t; haveToken.resolve(); }),
  });
  await c1.opened;
  await Promise.all([haveTicket.promise, haveToken.promise]);
  const rs = await c1.createBidirectionalStream({ body: encoder.encode('a') });
  for await (const _ of rs) { /* drain */ } // eslint-disable-line no-unused-vars
  await c1.close();
  await ep1.close();

  const ep2 = await listen(handler, {
    appTicketData: new Uint8Array([9, 9, 9]),
    endpoint: { tokenSecret },
  });
  const c2 = await connect(ep2.address, { sessionTicket: ticket, token });
  const es = await c2.createBidirectionalStream({ body: encoder.encode('b') });
  es.closed.catch(() => {});
  const info = await c2.opened;
  strictEqual(info.earlyDataAttempted, true);
  strictEqual(info.earlyDataAccepted, false);
  for await (const _ of es) { /* drain */ } // eslint-disable-line no-unused-vars
  await c2.close();
  await ep2.close();
}

// --- Argument validation: appTicketData must be an ArrayBufferView ---
await rejects(connect('127.0.0.1:1', { appTicketData: 'nope' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
await rejects(listen(() => {}, { appTicketData: 123 }), {
  code: 'ERR_INVALID_ARG_TYPE',
});

ok(true);

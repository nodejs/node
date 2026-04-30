// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Regression test: when a client QuicEndpoint has a session terminated
// by a stateless reset, subsequent sessions on the same endpoint must
// be able to complete their stream close handshake.
// Without the fix, the server-side stream.closed for session 2 never
// resolves and the test hangs.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');
const { QuicEndpoint } = await import('node:quic');

const encoder = new TextEncoder();

let sessionCount = 0;
const serverDone1 = Promise.withResolvers();
const serverDone2 = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  sessionCount++;
  const which = sessionCount;

  serverSession.onstream = mustCall(async (stream) => {
    const data = await bytes(stream);
    assert.ok(data.byteLength > 0);
    stream.writer.endSync();
    // For session 2 this hangs when the bug is present — the server
    // never receives the client's ACK for its FIN.
    await stream.closed;

    if (which === 1) serverSession.destroy();
    (which === 1 ? serverDone1 : serverDone2).resolve();
  });
}, 2), {
  onerror(err) { /* marks promises as handled */ },
});

// Both sessions share one endpoint — same source UDP address.
const clientEndpoint = new QuicEndpoint();

// Session 1: complete a full round-trip, then the server destroys
// without sending CONNECTION_CLOSE. The client sends a packet to the
// now-unknown DCID, which causes the server to send a stateless reset.
// The client receives the stateless reset and closes session 1.
const client1 = await connect(serverEndpoint.address, {
  endpoint: clientEndpoint,
  onerror: mustCall((err) => { assert.ok(err); }),
});
await client1.opened;

const s1 = await client1.createBidirectionalStream({
  body: encoder.encode('session1'),
});
for await (const _ of s1) { /* drain */ } // eslint-disable-line no-unused-vars
await s1.closed;

await serverDone1.promise;

// Trigger the stateless reset.
// eslint-disable-next-line no-unused-vars
const s1b = await client1.createBidirectionalStream({
  body: encoder.encode('trigger'),
});
await assert.rejects(client1.closed, { code: 'ERR_QUIC_TRANSPORT_ERROR' });

// Session 2: uses the same endpoint as session 1. The bug manifests
// as serverDone2 never resolving because the server's stream.closed
// for session 2 hangs.
const client2 = await connect(serverEndpoint.address, {
  endpoint: clientEndpoint,
  onerror(err) { /* marks promises as handled */ },
});
await client2.opened;

const s2 = await client2.createBidirectionalStream({
  body: encoder.encode('session2'),
});
for await (const _ of s2) { /* drain */ } // eslint-disable-line no-unused-vars
await s2.closed;

// If the bug is present, this never resolves and the test hangs.
await serverDone2.promise;

await serverEndpoint.close();
await clientEndpoint.close();

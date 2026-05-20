// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: diagnostics_channel stream and handshake events
// quic.session.handshake fires on handshake complete (not in
//        the channel list but we test the opened event path).
// quic.session.open.stream fires when a stream is created locally.
// quic.session.received.stream fires when a remote stream arrives.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();

// Fires when the client creates a stream.
dc.subscribe('quic.session.open.stream', mustCall((msg) => {
  ok(msg.stream);
  ok(msg.session);
  strictEqual(msg.direction, 'bidi', 'open.stream direction should be bidi');
}));

// Fires when the server receives a stream.
dc.subscribe('quic.session.received.stream', mustCall((msg) => {
  ok(msg.stream);
  ok(msg.session);
  strictEqual(msg.direction, 'bidi', 'received.stream direction should be bidi');
}));

// Fires when a stream is destroyed.
dc.subscribe('quic.stream.closed', mustCall((msg) => {
  ok(msg.stream);
  ok(msg.session);
  ok(msg.stats, 'stream.closed should include stats');
}, 2));

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await bytes(stream);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('diagnostics test'),
});

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars

await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);
await serverEndpoint.close();

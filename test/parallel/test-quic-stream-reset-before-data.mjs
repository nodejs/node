// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: resetStream() before any data is written.
// The stream is in the Ready state — no data has been sent by the
// client. The client calls resetStream() which sends RESET_STREAM
// to the server. The server receives the stream via onstream (the
// RESET_STREAM implicitly creates the bidi stream), and onreset
// fires. The server then sends data back on its side of the bidi
// stream, which the client reads — verifying that even when the
// client's send side is reset, the server can still use its send
// side and the client can still receive.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

const { ok, deepStrictEqual, strictEqual, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

// quic.stream.reset fires when a stream receives RESET_STREAM from the peer.
dc.subscribe('quic.stream.reset', mustCall((msg) => {
  ok(msg.stream, 'stream.reset should include stream');
  ok(msg.session, 'stream.reset should include session');
  ok(msg.error, 'stream.reset should include error');
}));

const encoder = new TextEncoder();
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    stream.onreset = mustCall((error) => {
      strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
      ok(error.message.includes('42'));

      // The client reset its send side, but the server can still
      // send data on its side of the bidi stream.
      stream.setBody(encoder.encode('response'));
    });

    // The stream's closed promise may reject because the client's
    // send side was reset. Either way, clean up.
    await rejects(stream.closed, {
      code: 'ERR_QUIC_APPLICATION_ERROR',
    });
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address, {
  onerror: mustNotCall(),
});
await clientSession.opened;

// Create a bidi stream but do NOT write any data.
const stream = await clientSession.createBidirectionalStream();

// Reset immediately — no data was ever written. This sends
// RESET_STREAM to the server which implicitly creates the bidi
// stream on the server side.
stream.resetStream(42n);

// The client should still be able to receive data from the server
// on the readable side of this bidi stream.
const received = await bytes(stream);
deepStrictEqual(Buffer.from(received), Buffer.from('response'));

// stream.closed rejects with the reset error (the client's send
// side was reset). Verify the error and consume the rejection.
await stream.closed.catch((error) => {
  strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
  ok(error.message.includes('42'));
});
await serverDone.promise;
await clientSession.close();
await serverEndpoint.close();

// Flags: --experimental-quic --no-warnings

// stream.destroy(err) emits RESET_STREAM on the wire even when the
// user never accessed `stream.writer`. Previously the wire frame was
// only emitted via the writer.fail path inside [kFinishClose], so
// streams that destroyed without ever creating a writer (e.g. used
// `setBody()` or never wrote at all) left the write side dangling on
// the wire and the peer kept the stream alive until the session-level
// idle timer fired.
//
// Verified by observing the server-side `onreset` callback. The wire
// code is the negotiated application's "internal error" code: for
// the test fixture's non-h3 ALPN (`quic-test`) the C++
// DefaultApplication reports `1n`, which propagates to the server
// as `ERR_QUIC_APPLICATION_ERROR` exposing `errorCode === 1n`.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverResetSeen = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    stream.onreset = mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_QUIC_APPLICATION_ERROR');
      // The DefaultApplication's internal error code is 0x1n.
      assert.strictEqual(err.errorCode, 1n);
      serverResetSeen.resolve();
    });

    // The peer's reset causes stream.closed to reject with the reset
    // error code.
    await assert.rejects(stream.closed, {
      code: 'ERR_QUIC_APPLICATION_ERROR',
    });
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Bidirectional stream with a body source set up front. No JS-side
// writer is accessed -- the body is consumed and pushed by the C++
// streaming source. Pre-B13, calling destroy() on this stream would
// not emit RESET_STREAM because the writer.fail path was the only
// emission site.
const stream = await clientSession.createBidirectionalStream({
  body: 'hello world',
});

const err = new Error('destroy without writer');
// Pre-attach the rejection assertion before destroying so the
// resulting `stream.closed` rejection isn't reported as unhandled.
const clientClosedAssertion = assert.rejects(stream.closed, err);

stream.destroy(err);

await Promise.all([clientClosedAssertion, serverResetSeen.promise]);

await clientSession.close();
await serverEndpoint.close();

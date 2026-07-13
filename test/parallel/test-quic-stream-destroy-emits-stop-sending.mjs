// Flags: --experimental-quic --no-warnings

// stream.destroy(err) emits STOP_SENDING on the wire so the peer
// stops sending data the local side is about to discard. Previously,
// destroy never sent STOP_SENDING - the readable side stayed open
// from the peer's perspective and they would keep transmitting until
// the session-level idle timer fired.
//
// Verified via the server-side writer's `canWrite` getter, which
// returns `null` once `state.writeEnded` is set. STOP_SENDING from
// the client triggers `Stream::ReceiveStopSending -> EndWritable` on
// the server, which sets `state.writeEnded = 1`.
//
// ngtcp2 packs RESET_STREAM before STOP_SENDING in the same packet
// regardless of the order our JS code makes the calls. The server
// processes RESET_STREAM first (firing `onreset`) and then
// STOP_SENDING. The observation must therefore be deferred until
// after the `onreset` callback returns so ngtcp2 can finish
// processing the packet. Capturing `writer.canWrite` synchronously
// inside `onreset` would always see the pre-STOP_SENDING value.
// Throwing inside `onreset` would also crash the process before
// STOP_SENDING could be processed.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverObservation = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const writer = stream.writer;
    // Sanity: the writer is active before the peer tears the stream
    // down, so canWrite is a boolean reflecting the initial
    // flow-control window.
    if (typeof writer.canWrite !== 'boolean') {
      serverObservation.reject(new Error(
        `expected initial writer.canWrite to be a boolean, ` +
        `got ${writer.canWrite}`));
      return;
    }
    stream.onreset = mustCall(() => {
      // Defer the writeEnded observation: ngtcp2 fires this callback
      // synchronously while processing the RESET_STREAM frame, before
      // it gets to the STOP_SENDING frame in the same packet. By the
      // next event loop tick the rest of the packet has been
      // processed and `Stream::ReceiveStopSending -> EndWritable` has
      // flipped `state.writeEnded` to 1, which makes the writer's
      // canWrite getter return null.
      setImmediate(() => {
        serverObservation.resolve(writer.canWrite);
      });
    });

    // The peer's reset causes stream.closed to reject.
    await assert.rejects(stream.closed, {
      code: 'ERR_QUIC_APPLICATION_ERROR',
    });
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: 'client-data',
});

const err = new Error('destroy with error');
const clientClosedAssertion = assert.rejects(stream.closed, err);

stream.destroy(err);

const observedCanWrite = await serverObservation.promise;
strictEqual(observedCanWrite, null);

await clientClosedAssertion;

clientSession.close();
await clientSession.closed;
await serverEndpoint.close();

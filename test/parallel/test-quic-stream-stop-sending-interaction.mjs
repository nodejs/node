// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: STOP_SENDING / RESET_STREAM interaction.
// When the peer sends STOP_SENDING, the local sending side is
//         notified — the stream closes on the sender side.
// Receiving STOP_SENDING automatically triggers RESET_STREAM
//         to the peer (ngtcp2 handles this internally). Verified by
//         the server's stream.closed rejecting with the error code.
// The error code from STOP_SENDING is copied to the automatic
//         RESET_STREAM — the server's stream.closed rejects with the
//         same code that was passed to stopSending().

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const stopCode = 77n;

const serverDone = Promise.withResolvers();
const clientStreamReady = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Wait for the client stream to be fully set up.
    await clientStreamReady.promise;

    // Send STOP_SENDING with a specific error code.
    stream.stopSending(stopCode);

    // Send data from server to client (the other direction is unaffected).
    const w = stream.writer;
    w.writeSync(encoder.encode('server data'));
    w.endSync();

    // The server's stream.closed rejects because
    // the client automatically sends RESET_STREAM in response to
    // STOP_SENDING. The error code matches the STOP_SENDING code.
    await rejects(stream.closed, (error) => {
      strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
      ok(error.message.includes(String(stopCode)));
      return true;
    });

    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('initial data'),
});
clientStreamReady.resolve();

// Read the server's data. The server→client direction is unaffected
// by STOP_SENDING on the client→server direction.
const received = await bytes(stream);
strictEqual(new TextDecoder().decode(received), 'server data');

// The client's stream.closed resolves. The STOP_SENDING caused
// the client's write side to end (ngtcp2 sends RESET_STREAM
// automatically), but from the client's JS perspective the stream
// completed: the read side got FIN from the server, and the write
// side was handled internally by ngtcp2. stream.closed only rejects
// on the side that receives the RESET_STREAM (the server).
await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);
await serverEndpoint.close();

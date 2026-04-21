// Flags: --experimental-quic --no-warnings

// Test: onreset callback error handling.
// A sync throw in stream.onreset destroys the STREAM (not the session)
// via safeCallbackInvoke. The stream.onerror fires with the original
// error, and stream.closed rejects. The session remains alive.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();
const testError = new Error('onreset throw');

const serverReady = Promise.withResolvers();
const serverStreamDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // The stream's onerror should fire with the throw from onreset.
    stream.onerror = mustCall((err) => {
      strictEqual(err, testError);
    });

    stream.onreset = () => {
      throw testError;
    };

    serverReady.resolve();

    // Stream closed rejects because the onreset throw destroyed it.
    await rejects(stream.closed, testError);
    serverStreamDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('trigger onstream'),
});

// Wait for the server to have the stream before resetting.
await serverReady.promise;
stream.resetStream(1n);

// Wait for the server stream to be destroyed by the onreset throw.
await serverStreamDone.promise;

// The client stream was reset. Destroy it explicitly to clean up
// (resetStream only shuts the write side; the read side is still open
// waiting for the server which won't send anything now).
stream.destroy();
await stream.closed;

// Close both sides.
await clientSession.close();
await serverEndpoint.close();

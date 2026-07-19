// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream.onerror callback behavior.
// * stream.onerror fires when stream is destroyed with error.
// * stream.onerror receives the original error as argument.
// * stream.closed rejects with the original error after onerror.
// * stream.onerror not called when destroy() has no error.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

{
  const stream = await clientSession.createBidirectionalStream({
    body: encoder.encode('will error'),
  });

  const testError = new Error('stream destroy error');

  let onerrorCalled = false;
  stream.onerror = mustCall((err) => {
    // Receives the original error.
    strictEqual(err, testError);
    onerrorCalled = true;
  });

  stream.destroy(testError);

  // The onerror was called synchronously during destroy.
  strictEqual(onerrorCalled, true);

  // The stream.closed rejects with the original error.
  await rejects(stream.closed, testError);
}

// The stream.onerror not called when destroy() has no error.
// Create a stream with no body — use the writer API so the server sees
// it and can close cleanly.
{
  const stream = await clientSession.createBidirectionalStream();
  const w = stream.writer;

  stream.onerror = mustNotCall('stream.onerror should not be called');

  // Send data so the server's onstream fires, then end.
  w.writeSync('no error');
  w.endSync();

  // Wait for the server to process and close its side.
  await serverDone.promise;

  // Now destroy without error.
  stream.destroy();

  // Closed should resolve (not reject).
  await stream.closed;
}

await clientSession.close();
await serverEndpoint.close();

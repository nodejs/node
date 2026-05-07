// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: graceful session close with open streams.
// session.close() with open streams waits for streams to close
//           before the session's closed promise resolves.
// After close() is called, no new streams can be created.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();

// -------------------------------------------------------------------
// close() waits for open streams to finish.
// -------------------------------------------------------------------
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const received = await bytes(stream);
      strictEqual(received.byteLength, 5);
      stream.writer.endSync();
      await stream.closed;
      serverDone.resolve();
    });
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  // Open a stream and send data.
  const stream = await clientSession.createBidirectionalStream({
    body: encoder.encode('hello'),
  });

  // Call close() while the stream is still open. The closed promise
  // should NOT resolve until the stream finishes.
  let closedResolved = false;
  const closePromise = clientSession.close();
  closePromise.then(mustCall(() => { closedResolved = true; }));

  // Wait for the stream to complete normally.
  await serverDone.promise;
  for await (const batch of stream) { /* drain server FIN */ } // eslint-disable-line no-unused-vars
  await stream.closed;

  // Now the closed promise should resolve.
  await closePromise;
  strictEqual(closedResolved, true);
  strictEqual(clientSession.destroyed, true);

  await serverEndpoint.close();
}

// -------------------------------------------------------------------
// No new streams after close() is called.
// -------------------------------------------------------------------
{
  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    await serverSession.closed;
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  clientSession.close();

  // Attempting to create a stream after close() should reject.
  await rejects(
    clientSession.createBidirectionalStream({
      body: encoder.encode('too late'),
    }),
    {
      code: 'ERR_INVALID_STATE',
    },
  );

  await clientSession.closed;
  await serverEndpoint.close();
}

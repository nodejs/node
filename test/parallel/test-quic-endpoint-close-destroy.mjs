// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: endpoint close and destroy lifecycle.
// endpoint.close() waits for active sessions to finish.
// endpoint.destroy() forcefully closes all sessions.
// endpoint.closing property reflects close state.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
const { setTimeout } = await import('node:timers/promises');
const { bytes } = await import('stream/iter');

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (serverSession) => {
    // Set onstream before awaiting anything so the callback isn't
    // missed if data arrives quickly.
    const streamDone = Promise.withResolvers();
    serverSession.onstream = mustCall(async (stream) => {
      await bytes(stream);
      stream.writer.endSync();
      await stream.closed;
      streamDone.resolve();
    });

    await serverSession.opened;

    // Before close, closing is false.
    strictEqual(serverEndpoint.closing, false);

    // Initiate endpoint close — it should wait for this session.
    serverEndpoint.close();

    // After close() is called, closing is true.
    strictEqual(serverEndpoint.closing, true);

    // The endpoint's closed promise should NOT resolve yet — session is open.
    let endpointClosed = false;
    serverEndpoint.closed.then(mustCall(() => { endpointClosed = true; }));

    // Give a tick to confirm endpoint hasn't closed yet.
    await setTimeout(100);
    strictEqual(endpointClosed, false);

    // Wait for the stream to complete, then close the session.
    await streamDone.promise;
    serverSession.close();
    serverDone.resolve();
  }), {
    transportParams: { maxIdleTimeout: 1 },
  });

  const clientSession = await connect(serverEndpoint.address, {
    transportParams: { maxIdleTimeout: 1 },
  });
  await clientSession.opened;

  // Send some data so the server session has work to do.
  const stream = await clientSession.createBidirectionalStream({
    body: encoder.encode('test'),
  });

  await Promise.all([serverDone.promise,
                     stream.closed,
                     serverEndpoint.closed]);

  strictEqual(serverEndpoint.destroyed, true);
}

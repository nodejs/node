// Flags: --experimental-quic --no-warnings

// Test: Verify that when a peer resets a stream after writer.endSync()
// is called, the stream.onerror handler fires and no unhandled rejection occurs.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverDone = Promise.withResolvers();

// Listen for unhandled rejections to catch regressions.
process.on('unhandledRejection', (reason) => {
  assert.fail(`Unhandled rejection: ${reason}`);
});

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Set up onerror to verify it fires on remote reset
    const errorFired = Promise.withResolvers();
    stream.onerror = mustCall((error) => {
      strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
      strictEqual(error.errorCode, 256n);
      errorFired.resolve();
    });

    stream.sendHeaders({ ':status': '200' });
    stream.writer.endSync();

    await errorFired.promise;
    await rejects(stream.closed);

    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
stream.resetStream(256n);
await rejects(stream.closed);

await serverDone.promise;
await clientSession.close();
await serverEndpoint.close();

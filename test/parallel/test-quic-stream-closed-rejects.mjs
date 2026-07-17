// Flags: --experimental-quic --no-warnings

// Test: stream.closed promise rejects on error.
// The server resets the stream, causing both sides' closed to reject
// with the application error code.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { ok, rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    stream.resetStream(1n);

    // The server's own stream.closed should also reject with the
    // reset error code.
    await rejects(stream.closed, (error) => {
      strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
      ok(error.message.includes('1'));
      return true;
    });

    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('will error'),
});

// Client's closed should reject with the reset error code.
await rejects(stream.closed, (error) => {
  strictEqual(error.code, 'ERR_QUIC_APPLICATION_ERROR');
  ok(error.message.includes('1'));
  return true;
});

await serverDone.promise;
await clientSession.close();
await serverEndpoint.close();

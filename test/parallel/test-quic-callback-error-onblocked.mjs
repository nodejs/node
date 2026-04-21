// Flags: --experimental-quic --no-warnings

// Test: onblocked callback error handling.
// A sync throw in stream.onblocked destroys the stream via
// safeCallbackInvoke. The stream.closed promise rejects with the error.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const testError = new Error('onblocked throw');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), {
  // Small stream window to trigger flow control blocking.
  transportParams: {
    maxIdleTimeout: 1,
    initialMaxStreamDataBidiRemote: 256,
  },
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
});
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();

stream.onblocked = mustCall(() => {
  throw testError;
});

// Body larger than the 256-byte flow control window triggers onblocked.
stream.setBody(new Uint8Array(4096));

// The stream's closed promise should reject with the error from the throw.
await rejects(stream.closed, testError);

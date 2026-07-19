// Flags: --experimental-quic --no-warnings

// Test: body: Promise rejection destroys the stream.
// When the body is a Promise that rejects, the stream should be
// destroyed with the rejection error.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), { transportParams: { maxIdleTimeout: 5 } });

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 5 },
});
await clientSession.opened;

const testError = new Error('promise body rejected');
const stream = await clientSession.createBidirectionalStream();

// Attach the closed handler BEFORE setBody so the rejection from
// stream.destroy(err) is caught before it becomes unhandled.
const closedPromise = rejects(stream.closed, testError);

stream.setBody(Promise.reject(testError));

// The stream should be destroyed with the rejection error.
await Promise.all([closedPromise, clientSession.closed]);
await serverEndpoint.close();

// Flags: --experimental-quic --no-warnings

// Test: async iterable source error destroys the stream.
// When the async iterable body source throws, the stream should be
// destroyed with the error and stream.closed should reject.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), { transportParams: { maxIdleTimeout: 1 } });

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
});
await clientSession.opened;

const testError = new Error('async source error');

async function* failingSource() {
  yield encoder.encode('partial ');
  throw testError;
}

const stream = await clientSession.createBidirectionalStream();

// Attach the closed handler BEFORE setBody so the rejection from
// stream.destroy(err) is caught before it becomes unhandled.
const closedPromise = rejects(stream.closed, testError);

stream.setBody(failingSource());

// The stream should be destroyed with the source error.
await Promise.all([closedPromise, clientSession.closed]);
await serverEndpoint.close();

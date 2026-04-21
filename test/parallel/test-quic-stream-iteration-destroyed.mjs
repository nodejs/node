// Flags: --experimental-quic --no-warnings

// Test: destroyed stream returns finished iterator.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('destroy test'),
});

// Destroy the stream immediately.
stream.destroy();

// Iterating a destroyed stream should immediately finish.
const iter = stream[Symbol.asyncIterator]();
const { done } = await iter.next();
strictEqual(done, true);

await stream.closed;
await clientSession.close();
await serverEndpoint.destroy();

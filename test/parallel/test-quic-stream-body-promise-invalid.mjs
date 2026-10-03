// Flags: --experimental-quic --no-warnings

// Test: body: Promise resolving to an invalid body destroys the stream.
// When the body is a Promise that resolves to an unsupported type, the
// stream should be destroyed with the same ERR_INVALID_ARG_TYPE that
// setBody() throws synchronously for that type.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

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

const stream = await clientSession.createBidirectionalStream();

assert.throws(() => stream.setBody(42), { code: 'ERR_INVALID_ARG_TYPE' });

const stream2 = await clientSession.createBidirectionalStream();

const closedPromise = assert.rejects(stream2.closed, {
  code: 'ERR_INVALID_ARG_TYPE',
});

stream2.setBody(Promise.resolve(42));

await Promise.all([closedPromise, clientSession.closed]);
await serverEndpoint.close();

// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: data received before the async iterator is pulled remains readable
// after a peer-initiated unidirectional stream closes.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const expected = encoder.encode('data before the first pull');
const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  const stream = await serverSession.createUnidirectionalStream({
    body: expected,
  });
  await stream.closed;
  serverSession.close();
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

clientSession.onstream = mustCall(async (stream) => {
  const iterator = stream[Symbol.asyncIterator]();

  // Delay the first pull until after the peer has sent FIN and the native
  // stream handle has been closed.
  await stream.closed;

  const received = await bytes(iterator);
  assert.deepStrictEqual(received, expected);
  clientSession.close();
  done.resolve();
});

await done.promise;
await clientSession.closed;
await serverEndpoint.close();

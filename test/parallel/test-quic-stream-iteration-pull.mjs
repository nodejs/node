// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: pull applies a transform to a QUIC stream.
// Verifies that pull() can process chunks from a QUIC stream through
// a synchronous transform function.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { deepStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes, pull } = await import('stream/iter');

const encoder = new TextEncoder();
const message = 'pull test';
const expected = encoder.encode(message);

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Use pull with an identity transform — pass chunks through.
    const transformed = pull(stream, (chunk) => {
      if (chunk === null) return null;
      return chunk;
    });
    const result = await bytes(transformed);
    deepStrictEqual(result, expected);

    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode(message),
});

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

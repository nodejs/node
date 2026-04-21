// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: body: Promise rejection and nested promise depth.
// Promise rejection during body configuration errors the stream.
// Nested promises are resolved up to the depth limit.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

// Nested promises — the body is resolved recursively up to
// depth 3. A Promise<Promise<string>> should work.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const received = await bytes(stream);
      strictEqual(
        new TextDecoder().decode(received),
        'nested promise',
      );
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }));

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  // Double-nested promise: Promise<Promise<string>>
  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(
    Promise.resolve(Promise.resolve('nested promise')),
  );

  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await Promise.all([stream.closed, serverDone.promise]);
  await clientSession.close();
  await serverEndpoint.close();
}

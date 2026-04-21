// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: body: nested promise resolution.
// Native promises auto-flatten, so Promise<Promise<string>> resolves
// to the inner string value.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

// Nested promises — native promises auto-flatten, so the
// resolved value is never itself a promise.
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

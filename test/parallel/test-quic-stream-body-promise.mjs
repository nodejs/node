// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: body from Promise.
// Promise<string> is awaited then configured as a string body.
// Promise<null> is awaited then closes the writable side.
// BODY-13 (Promise rejection) is not tested here because the rejected
// promise calls resetStream synchronously which may or may not cause
// the server's onstream to fire depending on timing.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { text, bytes } = await import('stream/iter');

let streamIdx = 0;
const totalStreams = 2;
const allDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const idx = streamIdx++;

    if (idx === 0) {
      // Promise<string> resolved to string data.
      const received = await text(stream);
      strictEqual(received, 'resolved string');
    } else if (idx === 1) {
      // Promise<null> closes the writable side.
      const received = await bytes(stream);
      strictEqual(received.byteLength, 0);
    }

    stream.writer.endSync();
    await stream.closed;

    if (streamIdx === totalStreams) {
      serverSession.close();
      allDone.resolve();
    }
  }, totalStreams);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Promise<string>
{
  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(Promise.resolve('resolved string'));
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// Promise<null>
{
  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(Promise.resolve(null));
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

await allDone.promise;
await clientSession.close();
await serverEndpoint.close();

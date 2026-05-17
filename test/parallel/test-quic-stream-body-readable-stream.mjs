// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: body from ReadableStream and stream.Readable.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { deepStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');
const { Readable } = await import('node:stream');

const encoder = new TextEncoder();
const message = 'readable stream body';
const expected = encoder.encode(message);

let serverStreamCount = 0;
const allDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    deepStrictEqual(received, expected);
    stream.writer.endSync();
    await stream.closed;
    if (++serverStreamCount === 2) {
      serverSession.close();
      allDone.resolve();
    }
  }, 2);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Web ReadableStream as body source.
{
  const rs = new ReadableStream({
    start(controller) {
      controller.enqueue(encoder.encode(message));
      controller.close();
    },
  });
  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(rs);
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// Node.js stream.Readable as body source.
{
  const readable = Readable.from([encoder.encode(message)]);
  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(readable);
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

await allDone.promise;
await clientSession.close();
await serverEndpoint.close();

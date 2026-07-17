// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: body from async iterable source.
// An async generator is used as the body source. The data is consumed
// via the streaming path in configureOutbound.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { deepStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const chunks = ['hello ', 'from ', 'async ', 'iterable'];
const expected = encoder.encode(chunks.join(''));

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    deepStrictEqual(received, expected);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

async function* generateChunks() {
  for (const chunk of chunks) {
    yield encoder.encode(chunk);
  }
}

const stream = await clientSession.createBidirectionalStream();
stream.setBody(generateChunks());

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

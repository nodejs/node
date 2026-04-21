// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: body from sync iterable source.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { deepStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const chunks = ['sync ', 'iterable ', 'source'];
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

// Create an array of Uint8Arrays as a sync iterable body.
const bodyChunks = chunks.map((c) => encoder.encode(c));
const stream = await clientSession.createBidirectionalStream();
stream.setBody(bodyChunks);

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: basic bidirectional stream data transfer.
// The client creates a bidi stream with a fixed body. The server reads the
// data via async iteration (using stream/iter bytes()), verifies integrity,
// then closes its write side of the stream. Both sides await stream.closed
// to ensure the stream is fully acknowledged before the session is torn down.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { deepStrictEqual, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const message = 'Hello from the client';
const encoder = new TextEncoder();
const decoder = new TextDecoder();
// Keep a separate copy for comparison — the body passed to
// createBidirectionalStream will have its ArrayBuffer transferred.
const body = encoder.encode(message);
const expected = encoder.encode(message);

const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);

    deepStrictEqual(received, expected);
    strictEqual(decoder.decode(received), message);

    // Close the server's write side of the bidi stream (FIN with no data)
    // so the stream is fully closed on both directions.
    stream.writer.endSync();

    // Wait for the stream to be fully closed before closing the session.
    await stream.closed;
    serverSession.close();
    done.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Create a bidi stream with the message as the body.
// For DefaultApplication, the server's onstream fires when data arrives.
const stream = await clientSession.createBidirectionalStream({
  body: body,
});

await Promise.all([stream.closed, done.promise]);
await clientSession.close();
await serverEndpoint.close();

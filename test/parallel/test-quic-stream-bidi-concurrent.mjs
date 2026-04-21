// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: multiple concurrent bidirectional streams on a single session.
// The client opens several bidi streams in parallel, each sending a
// distinct message. The server reads each stream independently and
// verifies data integrity. All streams and the session are closed
// cleanly after all transfers complete.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const numStreams = 5;

const messages = Array.from({ length: numStreams },
                            (_, i) => `message from stream ${i}`);

let serverStreamsReceived = 0;
const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    const text = decoder.decode(received);

    // Verify it's one of the expected messages.
    ok(messages.includes(text),
       `Unexpected message: ${text}`);

    stream.writer.endSync();
    await stream.closed;

    serverStreamsReceived++;
    if (serverStreamsReceived === numStreams) {
      serverSession.close();
      done.resolve();
    }
  }, numStreams);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Open all streams concurrently.
const clientStreams = await Promise.all(
  messages.map((msg) =>
    clientSession.createBidirectionalStream({
      body: encoder.encode(msg),
    }),
  ),
);

await Promise.all([done.promise, ...clientStreams.map((s) => s.closed)]);
await clientSession.close();
await serverEndpoint.close();

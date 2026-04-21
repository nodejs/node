// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: server-initiated bidirectional stream.
// The server creates a bidi stream and sends a body to the client.
// The client receives the data via its onstream handler and verifies
// integrity. This is the reverse of test-quic-stream-bidi-basic.mjs.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { deepStrictEqual, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const message = 'Hello from the server';
const encoder = new TextEncoder();
const decoder = new TextDecoder();
const expected = encoder.encode(message);

const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;

  const stream = await serverSession.createBidirectionalStream({
    body: encoder.encode(message),
  });

  // Drain the client's write side (client sends FIN with no data).
  for await (const batch of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

clientSession.onstream = mustCall(async (stream) => {
  const received = await bytes(stream);

  deepStrictEqual(received, expected);
  strictEqual(decoder.decode(received), message);

  // Close the client's write side so the stream fully closes.
  stream.writer.endSync();
  await stream.closed;

  clientSession.close();
  done.resolve();
});

await done.promise;
await serverEndpoint.close();

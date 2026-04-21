// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: server-initiated unidirectional stream.
// The server creates a uni stream and sends data to the client.
// The client receives the data via its onstream handler and verifies
// integrity. The receiving side should not have a usable writer.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { deepStrictEqual, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const message = 'server uni stream data';
const encoder = new TextEncoder();
const decoder = new TextDecoder();
const expected = encoder.encode(message);

const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;

  const stream = await serverSession.createUnidirectionalStream({
    body: encoder.encode(message),
  });

  // Uni stream has no readable side for the sender.
  await stream.closed;
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

clientSession.onstream = mustCall(async (stream) => {
  const received = await bytes(stream);

  deepStrictEqual(received, expected);
  strictEqual(decoder.decode(received), message);

  // The receiving side of a uni stream should not be writable.
  // The writer should be pre-closed.
  const w = stream.writer;
  strictEqual(w.desiredSize, null);

  await stream.closed;
  clientSession.close();
  done.resolve();
});

await done.promise;
await serverEndpoint.close();

// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: basic unidirectional stream data transfer.
// The client creates a unidirectional stream with a body. The server reads
// the data and verifies integrity. The unidirectional stream is write-only
// on the client side and read-only on the server side.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { deepStrictEqual, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const message = 'unidirectional payload';
const encoder = new TextEncoder();
const decoder = new TextDecoder();
const body = encoder.encode(message);
const expected = encoder.encode(message);

const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    strictEqual(stream.direction, 'uni');

    const received = await bytes(stream);
    deepStrictEqual(received, expected);
    strictEqual(decoder.decode(received), message);

    // The server side of a remote unidirectional stream is not writable.
    // The writer should be pre-closed (desiredSize returns null).
    const w = stream.writer;
    strictEqual(w.desiredSize, null);
    strictEqual(w.endSync(), 0);

    await stream.closed;
    serverSession.close();
    done.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createUnidirectionalStream({ body });
strictEqual(stream.direction, 'uni');

// The client-side uni stream is write-only — async iteration yields nothing.
const iter = stream[Symbol.asyncIterator]();
const { done: iterDone } = await iter.next();
strictEqual(iterDone, true);

await done.promise;
// The server closed its session, delivering CONNECTION_CLOSE to the client.
// The client session enters the draining period, after which all streams
// and the session itself close cleanly.
await stream.closed;
await clientSession.close();
await serverEndpoint.close();

// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream.setBody() after creation.
// Creates a bidirectional stream without an initial body, then attaches
// a body via setBody(). The server reads the data and verifies integrity.
// Also verifies that calling setBody() a second time throws.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { deepStrictEqual, strictEqual, throws } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const message = 'body set after creation';
const expected = encoder.encode(message);

const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);

    deepStrictEqual(received, expected);
    strictEqual(decoder.decode(received), message);

    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    done.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Create a stream with no body.
const stream = await clientSession.createBidirectionalStream();

// Attach a body after creation.
stream.setBody(encoder.encode(message));

// Calling setBody() again should throw.
throws(() => {
  stream.setBody(encoder.encode('second body'));
}, {
  code: 'ERR_INVALID_STATE',
});

await Promise.all([stream.closed, done.promise]);
await clientSession.close();
await serverEndpoint.close();

// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: small flow control window blocks sender, resumes after FC
// update.
// With a very small initialMaxStreamDataBidiRemote, the sender
// blocks when the window is exhausted. The transfer completes
// successfully after the receiver extends the window.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const dataLength = 8192;
const data = new Uint8Array(dataLength);
for (let i = 0; i < dataLength; i++) data[i] = i & 0xff;

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Read all data — this extends the flow control window.
    const received = await bytes(stream);
    strictEqual(received.byteLength, dataLength);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  // Very small window — sender will block multiple times.
  transportParams: { initialMaxStreamDataBidiRemote: 128 },
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
stream.setBody(data);

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars

await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);

await serverEndpoint.close();

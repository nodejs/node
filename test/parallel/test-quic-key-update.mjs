// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: session.updateKey() initiates key update, data continues
// flowing.
// After calling updateKey(), the session transitions to new encryption
// keys. Existing and new streams should continue to work normally.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const dataLength = 1024;
const data = new Uint8Array(dataLength);
for (let i = 0; i < dataLength; i++) data[i] = i & 0xff;

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    strictEqual(received.byteLength, dataLength);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Initiate key update before sending data.
clientSession.updateKey();

// Open a stream and send data — should work with new keys.
const stream = await clientSession.createBidirectionalStream();
stream.setBody(data);

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

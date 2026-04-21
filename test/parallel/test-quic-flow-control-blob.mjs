// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: blob body larger than stream data window.
// A Blob body that exceeds the initial stream data window should
// still complete successfully — ngtcp2 handles the flow control
// extensions transparently for one-shot body sources.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { deepStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

// 8KB blob, 1KB stream window — requires flow control extension.
const data = new Uint8Array(8192);
for (let i = 0; i < data.length; i++) data[i] = i & 0xFF;
const blob = new Blob([data]);

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    deepStrictEqual(received, data);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  transportParams: { initialMaxStreamDataBidiRemote: 1024 },
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: blob,
});

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

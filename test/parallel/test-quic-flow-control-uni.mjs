// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: uni stream flow control.
// initialMaxStreamDataUni limits the flow control window for
// unidirectional streams. Data transfer still completes because
// the receiver extends the window.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes, drainableProtocol: dp } = await import('stream/iter');

const encoder = new TextEncoder();
const message = 'x'.repeat(4096);  // 4KB, larger than the 1KB window
const expected = encoder.encode(message);

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    strictEqual(received.byteLength, expected.byteLength);
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  transportParams: { initialMaxStreamDataUni: 1024 },
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createUnidirectionalStream({
  highWaterMark: 512,
});
const w = stream.writer;

const chunkSize = 256;
for (let offset = 0; offset < expected.byteLength; offset += chunkSize) {
  const chunk = expected.slice(offset, offset + chunkSize);
  while (!w.writeSync(chunk)) {
    const drain = w[dp]();
    if (drain) await drain;
  }
}
w.endSync();

await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

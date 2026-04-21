// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// writer.writeSync() must not detach the caller's underlying
// ArrayBuffer. The bytes from each write are copied into an internal
// buffer, so the caller's source ArrayBuffer remains live and may be
// reused, mutated, or sliced into additional views — including
// successive subarrays of the same Uint8Array.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

// Eight bytes split into two 4-byte halves.
const source = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);
const sourceCopy = source.slice();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    // Server receives the original 8 bytes in order, regardless of
    // any caller-side mutation that happens after writeSync returns.
    strictEqual(received.length, sourceCopy.length);
    for (let i = 0; i < sourceCopy.length; i++) {
      strictEqual(received[i], sourceCopy[i],
                  `byte ${i} mismatch: got ${received[i]}, ` +
                  `expected ${sourceCopy[i]}`);
    }
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
const writer = stream.writer;

// First half. The underlying ArrayBuffer must stay live after the
// write so that the caller can build the next view from it.
writer.writeSync(source.subarray(0, 4));
strictEqual(source.buffer.detached, false,
            'source ArrayBuffer must not be detached after writeSync');
strictEqual(source.byteLength, 8,
            'source view must remain usable after writeSync');

// Second half — slicing into the same backing buffer must succeed.
writer.writeSync(source.subarray(4, 8));
strictEqual(source.buffer.detached, false,
            'source ArrayBuffer must remain live after second writeSync');

// The C++ layer has already copied the bytes by the time writeSync
// returned. Mutating the source here must not affect the data the
// peer ultimately observes.
for (let i = 0; i < source.length; i++) source[i] = 0;
ok(source.every((b) => b === 0), 'source mutation should succeed');

writer.endSync();

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: a FIN that does not fit in the current packet is still sent.
//
// For a fixed body the final data and the FIN go out as separate STREAM
// frames, packed into the same packet where possible. When the data leaves
// only a few bytes free, the FIN-only frame does not fit and the packet is
// sent without it. The stream must then be retried, or the peer never sees
// the end of the stream.
//
// Which body size leaves too little room depends on per-packet overhead, so
// this sweeps sizes around one packet's payload, one stream at a time.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const kMinSize = 1000;
const kMaxSize = 1300;
const kCount = kMaxSize - kMinSize + 1;

const received = [];
let next = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const data = await bytes(stream);
    received.push(data.byteLength);
    next.resolve();
  }, kCount);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;
const closed = clientSession.closed.then(() => 'closed', () => 'closed');

for (let size = kMinSize; size <= kMaxSize; size++) {
  const stream = await clientSession.createUnidirectionalStream();
  stream.setBody(new Uint8Array(size));
  const result = await Promise.race([next.promise.then(() => 'ok'), closed]);
  assert.strictEqual(result, 'ok',
                     `session closed before the FIN of a ${size}-byte ` +
                     'stream arrived');
  next = Promise.withResolvers();
  assert.strictEqual(received.at(-1), size);
}

assert.strictEqual(received.length, kCount);

await clientSession.close();
await serverEndpoint.close();

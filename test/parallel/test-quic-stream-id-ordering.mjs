// Flags: --experimental-quic --no-warnings

// Test: stream IDs are strictly increasing and unique.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();
const serverDone = Promise.withResolvers();
let serverStreamCount = 0;

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
    stream.writer.endSync();
    await stream.closed;
    if (++serverStreamCount === 10) {
      serverSession.close();
      serverDone.resolve();
    }
  }, 10);
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const ids = [];
for (let i = 0; i < 10; i++) {
  const stream = await clientSession.createBidirectionalStream({
    body: encoder.encode(`stream ${i}`),
  });
  ids.push(stream.id);
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await stream.closed;
}

// Verify IDs are strictly increasing.
for (let i = 1; i < ids.length; i++) {
  ok(ids[i] > ids[i - 1],
     `Stream ID ${ids[i]} should be > ${ids[i - 1]}`);
}

// Verify all IDs are unique.
const uniqueIds = new Set(ids);
strictEqual(uniqueIds.size, ids.length);

await Promise.all([serverDone.promise, clientSession.closed]);
await serverEndpoint.close();

// Flags: --experimental-quic --no-warnings

// Test: write with aborted signal rejects immediately.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
const w = stream.writer;

// Create an already-aborted signal.
const ac = new AbortController();
ac.abort(new Error('already aborted'));

// write() with an already-aborted signal should reject immediately.
await rejects(
  w.write(encoder.encode('data'), { signal: ac.signal }),
  { message: 'already aborted' },
);

// The writer should still be usable for normal writes.
w.writeSync(encoder.encode('ok'));
w.endSync();

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

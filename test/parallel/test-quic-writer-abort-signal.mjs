// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: write with aborted signal rejects immediately.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

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

const stream = await clientSession.createBidirectionalStream({ budget: 1024 });
const w = stream.writer;

// Create an already-aborted signal.
const signal = AbortSignal.abort(new Error('already aborted'));

// write() with an already-aborted signal should reject immediately.
await assert.rejects(
  w.write(encoder.encode('data'), { signal }),
  { message: 'already aborted' },
);

// A pre-aborted end leaves the writer open.
await assert.rejects(
  w.end({ signal }),
  { message: 'already aborted' },
);

// Once end has started, aborting that call rejects only the operation. The
// stream still closes after the accepted pending write reaches capacity.
assert.strictEqual(w.writeSync(new Uint8Array(1024)), true);
const pendingWrite = w.write(encoder.encode('data'));
const controller = new AbortController();
const reason = new Error('end aborted while draining');
const abortedEnd = w.end({ signal: controller.signal });
controller.abort(reason);

await assert.rejects(abortedEnd, (error) => error === reason);
await pendingWrite;
assert.strictEqual(await w.end(), 1028);

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

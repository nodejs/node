// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Regression test for https://github.com/nodejs/node/issues/64290
// When a stream writer has a pending drain promise and the remote peer
// resets the stream, the rejected drain promise must NOT surface as an
// unhandled rejection.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';
import { setImmediate as tick } from 'node:timers/promises';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { drainableProtocol } = await import('stream/iter');

// The test fails if any unhandled rejection fires.
process.on('unhandledRejection',
           mustNotCall('unexpected unhandled rejection'));

const serverStreamReady = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall((stream) => {
    serverStreamReady.resolve({ stream, session: serverSession });
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
const writer = stream.writer;

// Write a small initial chunk so the server materializes the stream.
writer.writeSync(new Uint8Array([1]));

const { stream: serverStream, session: serverSession } =
  await serverStreamReady.promise;

// Fill the write buffer to create backpressure. After this,
// writeDesiredSize should be <= 0 and canWrite should be false.
const chunk = new Uint8Array(64 * 1024);
while (writer.canWrite) {
  if (!writer.writeSync(chunk)) break;
}

// Create a drain wakeup via the drainable protocol. This simulates
// what the stream/iter infrastructure does when checking for
// backpressure. We deliberately do NOT await the returned promise —
// that is the whole point of the test.
const drainPromise = writer[drainableProtocol]();
assert.ok(drainPromise instanceof Promise,
          'expected a drain promise (buffer should be full)');

// Suppress the expected rejection on both sides' closed promises so
// they do not interfere with the unhandledRejection check.
const clientClosed = stream.closed.catch(() => {});
const serverClosed = serverStream.closed.catch(() => {});

// Have the server send STOP_SENDING. This triggers kStopSending on
// the client writer, which rejects the unobserved drain promise.
// Without the fix this surfaces as an unhandled rejection.
serverStream.stopSending(1n);
serverStream.writer.endSync();

// Give the event loop time to process the frame and fire any
// unhandled-rejection events.
await tick();
await tick();

// Clean up.
await Promise.all([clientClosed, serverClosed]);
serverSession.close();
await clientSession.close();
await serverEndpoint.close();

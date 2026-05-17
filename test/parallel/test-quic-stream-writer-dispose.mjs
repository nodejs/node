// Flags: --experimental-quic --no-warnings

// Test: writer Symbol.dispose.
// Symbol.dispose calls fail() if the writer is not already closed/errored.
// After disposal, the writer is in an errored state.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const transportParams = { maxIdleTimeout: 1 };

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  // The server session will close via idle timeout because the client
  // resets the stream before any data is sent.
  await serverSession.closed;
}), { transportParams });

const clientSession = await connect(serverEndpoint.address, {
  transportParams,
});
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
const w = stream.writer;

// Writer is active — desiredSize should be a number (not null).
strictEqual(typeof w.desiredSize, 'number');

// Symbol.dispose calls fail() if not already closed/errored.
w[Symbol.dispose]();

// After dispose, writer should be errored.
strictEqual(w.desiredSize, null);
strictEqual(w.writeSync(encoder.encode('x')), false);

// stream.closed resolves because fail() with default code 0
// is treated as a clean close (no error).
// The session will close via idle timeout or CONNECTION_CLOSE.
await Promise.all([stream.closed, clientSession.closed]);
await serverEndpoint.close();

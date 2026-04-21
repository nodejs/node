// Flags: --experimental-quic --no-warnings

// Test: peer STOP_SENDING transitions writer to errored state.
// After the server calls stopSending(), the client's writer should
// become errored — desiredSize is null, writeSync returns false.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';
import { setTimeout } from 'node:timers/promises';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverReady = Promise.withResolvers();
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Tell the client to stop sending.
    stream.stopSending(1n);
    serverReady.resolve();
    stream.writer.endSync();
    await rejects(stream.closed, mustCall((err) => {
      assert.ok(err);
      return true;
    }));
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
const w = stream.writer;
w.writeSync(encoder.encode('initial data'));

// Wait for the server to send STOP_SENDING.
await serverReady.promise;

// Give a moment for the STOP_SENDING to propagate.
await setTimeout(100);

// After STOP_SENDING, the writer should be in an errored state.
// writeSync returns false (refuses to accept data).
strictEqual(w.writeSync(encoder.encode('rejected')), false);

// The stream closes after the server sends FIN.
await Promise.all([serverDone.promise, stream.closed]);
await clientSession.close();
await serverEndpoint.close();

// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: bidirectional stream echo.
// The client sends a message, the server reads it and echoes it back.
// Both directions of the bidi stream carry data and are properly FIN'd.
// Verifies that both client and server can read and write on the same stream.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const message = 'ping from client';
const encoder = new TextEncoder();
const decoder = new TextDecoder();

const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Read client's data.
    const received = await bytes(stream);

    // Echo it back and close the write side.
    const w = stream.writer;
    w.writeSync(received);
    w.endSync();

    await stream.closed;
    serverSession.close();
    done.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const body = encoder.encode(message);
const stream = await clientSession.createBidirectionalStream({ body });

// Read the echoed response from the server.
const echoed = await bytes(stream);
strictEqual(decoder.decode(echoed), message);

await Promise.all([stream.closed, done.promise]);
await clientSession.close();
await serverEndpoint.close();

// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: concurrent connections from multiple clients.
// Multiple clients connect to the same server simultaneously and each
// exchanges data successfully.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const numClients = 5;
let serverStreamCount = 0;
const allDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Echo back the data.
    const w = stream.writer;
    w.writeSync(await bytes(stream));
    w.endSync();
    await stream.closed;
    if (++serverStreamCount === numClients) {
      allDone.resolve();
    }
  });
}, numClients));

// Connect all clients concurrently.
const clientPromises = [];
for (let i = 0; i < numClients; i++) {
  clientPromises.push((async () => {
    const cs = await connect(serverEndpoint.address, { reuseEndpoint: false });
    await cs.opened;
    const message = `client ${i}`;
    const stream = await cs.createBidirectionalStream({
      body: encoder.encode(message),
    });
    const received = await bytes(stream);
    strictEqual(new TextDecoder().decode(received), message);
    await stream.closed;
    cs.close();
    await cs.closed;
  })());
}

await Promise.all([...clientPromises, allDone.promise]);
await serverEndpoint.close();

// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: half-close on a bidirectional stream.
// The client sends a body and closes its write side (FIN). While the
// client's writable side is closed, the server's writable side remains
// open. The server reads all client data, then sends a response back.
// The client reads the server's response and verifies both payloads.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const decoder = new TextDecoder();
const clientMessage = 'request from client';
const serverMessage = 'response from server';

const done = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Read the client's data (client has already sent FIN).
    const received = await bytes(stream);
    strictEqual(decoder.decode(received), clientMessage);

    // The server's writable side is still open. Send a response.
    const w = stream.writer;
    w.writeSync(encoder.encode(serverMessage));
    w.endSync();

    await stream.closed;
    serverSession.close();
    done.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Create a stream with a body -- this sends FIN after the body.
const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode(clientMessage),
});

// The client's writable side is closed (FIN sent with body), but
// the readable side is still open. Read the server's response.
const response = await bytes(stream);
strictEqual(decoder.decode(response), serverMessage);

await Promise.all([stream.closed, done.promise]);
await clientSession.close();
await serverEndpoint.close();

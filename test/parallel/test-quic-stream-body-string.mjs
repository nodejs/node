// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: body: string sends UTF-8 encoded data.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { text } = await import('stream/iter');

const message = 'Hello from a string body! 🎉 Unicode works.';

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await text(stream);
    strictEqual(received, message);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// setBody with a string — configureOutbound handles this via
// Buffer.from(body, 'utf8').
const stream = await clientSession.createBidirectionalStream();
stream.setBody(message);

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

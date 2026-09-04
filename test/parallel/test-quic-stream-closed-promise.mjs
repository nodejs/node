// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream.closed promise resolves after normal completion.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import { dump } from 'node:stream/iter';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await bytes(stream);
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('normal close'),
});

await dump(stream);

// Closed should resolve (not reject).
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

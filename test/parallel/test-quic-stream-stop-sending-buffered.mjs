// Flags: --experimental-quic --no-warnings

// A peer STOP_SENDING must unschedule buffered outbound data.

import { hasQuic, mustCall, skip } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { connect, listen } = await import('../common/quic.mjs');

const serverStreamReady = Promise.withResolvers();
const clientBuffered = Promise.withResolvers();
const serverReset = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    serverStreamReady.resolve();
    await clientBuffered.promise;

    const closed = assert.rejects(stream.closed, {
      code: 'ERR_QUIC_APPLICATION_ERROR',
    });
    stream.stopSending(1n);
    stream.writer.endSync();
    await closed;
    serverSession.close();
    serverReset.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
const clientClosed = stream.closed.catch(() => {});
const writer = stream.writer;
writer.writeSync(new Uint8Array([1]));
await serverStreamReady.promise;

writer.writeSync(new Uint8Array(64 * 1024));
clientBuffered.resolve();

await serverReset.promise;

await Promise.all([clientClosed, clientSession.closed]);
await serverEndpoint.close();

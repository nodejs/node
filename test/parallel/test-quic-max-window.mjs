// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: maxStreamWindow and maxWindow limits.
// maxStreamWindow limits per-stream receive window.
// maxWindow limits session-level receive window.
// With smaller windows, the transfer should still complete but may
// require more flow control updates.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const dataLength = 8192;

// maxStreamWindow limits per-stream window.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const received = await bytes(stream);
      strictEqual(received.byteLength, dataLength);
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }), {
    // Small per-stream receive window.
    maxStreamWindow: 1024,
  });

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(new Uint8Array(dataLength));
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);
  await serverEndpoint.close();
}

// maxWindow limits session-level window.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const received = await bytes(stream);
      strictEqual(received.byteLength, dataLength);
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }), {
    // Small session-level receive window.
    maxWindow: 2048,
  });

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream();
  stream.setBody(new Uint8Array(dataLength));
  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await Promise.all([stream.closed, serverDone.promise, clientSession.closed]);
  await serverEndpoint.close();
}

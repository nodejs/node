// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: flow control transport parameters.
// initialMaxData limits total connection-level data.
// initialMaxStreamDataBidiLocal limits stream data for locally
//         initiated bidi streams (server perspective for server-opened).
// initialMaxStreamDataBidiRemote limits stream data for remotely
//         initiated bidi streams (server perspective for client-opened).
// These tests verify that data transfers complete successfully even when
// flow control windows are very small, proving that flow control extension
// (MAX_DATA / MAX_STREAM_DATA) works correctly.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes, drainableProtocol: dp } = await import('stream/iter');

const encoder = new TextEncoder();

// Small initialMaxStreamDataBidiRemote — limits how much the
// client can send initially before the server extends flow control.
{
  const message = 'a]'.repeat(2048);  // 4KB, larger than the 1KB window
  const expected = encoder.encode(message);

  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const received = await bytes(stream);
      strictEqual(received.byteLength, expected.byteLength);
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }), {
    // Very small stream window — forces multiple flow control extensions.
    transportParams: { initialMaxStreamDataBidiRemote: 1024 },
  });

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream({
    highWaterMark: 512,
  });
  const w = stream.writer;

  // Write in small chunks, respecting backpressure.
  const chunkSize = 256;
  for (let offset = 0; offset < expected.byteLength; offset += chunkSize) {
    const chunk = expected.slice(offset, offset + chunkSize);
    while (!w.writeSync(chunk)) {
      const drain = w[dp]();
      if (drain) await drain;
    }
  }
  w.endSync();

  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await Promise.all([stream.closed, serverDone.promise]);
  await clientSession.close();
  await serverEndpoint.close();
}

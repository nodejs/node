// Flags: --experimental-quic --no-warnings

// session.destroy() with a close code sends a CONNECTION_CLOSE, which puts
// the connection into the closing state and ends all of its streams with it
// (RFC 9000, Section 10.2). A stream that is still open must therefore not
// be reset with a RESET_STREAM of its own ahead of the CONNECTION_CLOSE: the
// peer would see the stream fail, with an unrelated error code, before it
// learns how the session closed.
// Refs: https://github.com/nodejs/node/issues/67278

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

async function test(error, closeOptions, checkServerClosed) {
  const serverReceived = Promise.withResolvers();
  const serverClosed = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverClosed.resolve(checkServerClosed(serverSession.closed));
    serverSession.onstream = mustCall(async (stream) => {
      stream.onreset = mustNotCall('the stream must not be reset');
      stream.closed.catch(() => {});
      try {
        for await (const chunk of stream) {
          assert.ok(chunk.length > 0);
          serverReceived.resolve();
        }
      } catch {
        // The stream ends with the session.
      }
    });
  }));

  const resetFrames = [];
  const clientSession = await connect(serverEndpoint.address, {
    reuseEndpoint: false,
    qlog: true,
    onqlog(data) {
      for (const [frame] of String(data).matchAll(/"frame_type":"reset_stream"/g)) {
        resetFrames.push(frame);
      }
    },
  });
  await clientSession.opened;

  // A long-lived stream that is still open when the session is destroyed.
  const stream = await clientSession.createUnidirectionalStream();
  stream.closed.catch(() => {});
  stream.writer.write(new Uint8Array([1, 2, 3]));
  await serverReceived.promise;

  clientSession.destroy(error, closeOptions);
  await serverClosed.promise;

  assert.deepStrictEqual(resetFrames, []);
  await serverEndpoint.close();
}

// No error: the peer sees the session close cleanly with code 0.
await test(undefined, { code: 0, type: 'application' }, (closed) => closed);

// With an error, the peer sees the close code, not a stream reset.
await test(new Error('boom'), { code: 5n, type: 'application' },
           (closed) => assert.rejects(closed, {
             code: 'ERR_QUIC_APPLICATION_ERROR',
           }));

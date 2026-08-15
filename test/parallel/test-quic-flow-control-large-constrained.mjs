// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: high volume transfer through a deliberately constrained flow
// control window.
//
// The existing flow control tests either move a small amount of data through
// a tiny window, or a large amount of data through the default (large)
// window. Neither combination stresses the credit accounting: a bug that
// mis-credits a few bytes per window extension is invisible over 8KB but
// fatal over several megabytes.
//
// Here the payload is orders of magnitude larger than both the stream-level
// and connection-level receive windows, so completing the transfer requires
// hundreds of MAX_STREAM_DATA / MAX_DATA extensions. If any extension
// under-credits, the transfer stalls and the test times out; if any
// over-credits or misorders data, the hash check fails.

import { hasQuic, skip, mustCall, mustCallAtLeast } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, makePayload, hashBytes } =
  await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

// 4 MB through a 16 KB stream window and a 32 KB connection window. The
// windows are capped via maxStreamWindow/maxWindow as well, otherwise
// ngtcp2's window auto-tuning grows them and the transfer stops being
// flow-control bound.
const kTotal = 4 * 1024 * 1024;
const kStreamWindow = 16 * 1024;
const kConnWindow = 32 * 1024;

// Guard: the point of this test is that the window has to be recycled many
// times over. If someone lowers kTotal or raises the windows to speed the
// test up, this makes the loss of coverage explicit rather than silent.
assert.ok(kTotal / kConnWindow >= 100,
          'payload must require >=100 connection window refills');
assert.ok(kTotal / kStreamWindow >= 100,
          'payload must require >=100 stream window refills');

const transportParams = {
  initialMaxStreamDataBidiRemote: kStreamWindow,
  initialMaxStreamDataUni: kStreamWindow,
  initialMaxData: kConnWindow,
};
const windowCaps = {
  maxStreamWindow: kStreamWindow,
  maxWindow: kConnWindow,
};

// Bidirectional: client sends a large body, server verifies it.
{
  const payload = makePayload(kTotal);
  const expectedHash = hashBytes(payload);
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const received = await bytes(stream);
      assert.strictEqual(received.byteLength, kTotal);
      // Order-sensitive hash: catches duplicated, dropped, or reordered
      // regions that preserve the total length.
      assert.strictEqual(hashBytes(received), expectedHash);
      stream.writer.endSync();
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }), { transportParams, ...windowCaps });

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  const stream = await clientSession.createBidirectionalStream();

  // Assert the sender actually hit the window. Without this the test could
  // pass trivially if the windows were silently widened, and we would lose
  // the coverage without noticing.
  stream.onblocked = mustCallAtLeast(1);

  stream.setBody(payload);

  for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
  await Promise.all([stream.closed, serverDone.promise]);
  await clientSession.close();
  await serverEndpoint.close();
}

// Unidirectional: same volume, exercises the uni credit path, which uses
// different transport parameters and a different stream type.
{
  const payload = makePayload(kTotal, 7);
  const expectedHash = hashBytes(payload);
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall((serverSession) => {
    serverSession.onstream = mustCall(async (stream) => {
      const received = await bytes(stream);
      assert.strictEqual(received.byteLength, kTotal);
      assert.strictEqual(hashBytes(received), expectedHash);
      await stream.closed;
      serverSession.close();
      serverDone.resolve();
    });
  }), { transportParams, ...windowCaps });

  const clientSession = await connect(serverEndpoint.address);
  await clientSession.opened;

  const stream = await clientSession.createUnidirectionalStream();
  stream.onblocked = mustCallAtLeast(1);
  stream.setBody(payload);

  await Promise.all([stream.closed, serverDone.promise]);
  await clientSession.close();
  await serverEndpoint.close();
}

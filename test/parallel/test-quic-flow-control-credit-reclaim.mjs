// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: connection-level flow control credit is reclaimed for inbound data
// that is never consumed.
//
// Every byte QUIC delivers to us is charged against both the stream-level
// and the connection-level receive window. The stream-level window dies with
// the stream, but the connection-level window (`initialMaxData`) is shared by
// every stream on the session and is only replenished when we send MAX_DATA.
//
// If a stream is destroyed while inbound data is still sitting unread, that
// credit must still be returned, otherwise the session's receive window
// shrinks a little on every such stream until the connection deadlocks.
//
// This test destroys many streams without reading their responses, moving far
// more data in total than `initialMaxData` allows to be outstanding at once.
// It only completes if the credit is reclaimed on destroy.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

// Each response is 16 KB and the connection-level window is 64 KB, so the
// session can only ever have 4 unread responses outstanding. Running 20
// streams pushes 320 KB total -- 5x the window -- which can only work if the
// window is replenished as each stream is discarded.
const kPayloadSize = 16 * 1024;
const kInitialMaxData = 64 * 1024;
const kStreamCount = 20;

const payload = encoder.encode('a'.repeat(kPayloadSize));

const serverEndpoint = await listen(mustCall((serverSession) => {
  // One response per client stream, plus the final health-check stream.
  serverSession.onstream = mustCall((stream) => {
    // Respond and immediately finish. We never read the request body.
    stream.setBody(payload);
  }, kStreamCount + 1);
}));

const clientSession = await connect(serverEndpoint.address, {
  transportParams: {
    // Connection-level receive window, shared by all streams.
    initialMaxData: kInitialMaxData,
    // Give each individual stream enough room for a whole response so that
    // the connection-level window is the only thing that can throttle us.
    initialMaxStreamDataBidiLocal: kPayloadSize * 2,
  },
});
await clientSession.opened;

for (let i = 0; i < kStreamCount; i++) {
  const stream = await clientSession.createBidirectionalStream({
    body: encoder.encode('request'),
  });

  // Wait until the whole response has actually been received, so that the
  // bytes are genuinely holding flow control credit at the point we destroy
  // the stream. If credit is leaked, this loop is where later iterations
  // stall: the server runs out of connection-level window and can no longer
  // send, so bytesReceived never reaches the payload size.
  while (stream.stats.bytesReceived < kPayloadSize) {
    await new Promise((resolve) => setTimeout(resolve, 5));
  }

  // Destroy with the response still buffered and unread. This is the case
  // that used to leak: the backpressure listener is detached, so nothing
  // would ever return the credit for these bytes.
  stream.destroy();
}

// Getting here at all is the assertion -- a leak manifests as a timeout
// above. Verify the session is still healthy and able to move data, which
// confirms the window was replenished rather than merely limping along.
{
  const stream = await clientSession.createBidirectionalStream({
    body: encoder.encode('request'),
  });
  let received = 0;
  for await (const chunks of stream) {
    for (const chunk of chunks) received += chunk.byteLength;
  }
  assert.strictEqual(received, kPayloadSize);
}

await clientSession.close();
await serverEndpoint.close();

// Sanity check that the test actually exercised the intended path.
assert.ok(kPayloadSize * kStreamCount > kInitialMaxData,
          'test must move more data than the connection window allows');

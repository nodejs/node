// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: destroying a locally-initiated stream with data still in flight does
// not resurrect it.
//
// When a stream is destroyed, its ngtcp2 stream user data is cleared. Inbound
// STREAM frames the peer had already put in flight then arrive for a stream
// the receiver no longer tracks. Treating "no user data" as "a stream I have
// not seen before" is wrong for a stream we initiated ourselves: we are the
// only party that can create it, so the absence of a record means we
// destroyed it, not that it is new.
//
// Getting this wrong is visible from JavaScript in two ways, both asserted
// here:
//
//   * the session reports far more locally-opened streams than were actually
//     opened, because a fresh Stream is created for every frame still in
//     flight
//   * the application receives onstream events for streams it initiated and
//     already destroyed, which is a contract violation -- onstream is for
//     peer-initiated streams
//
// It also eventually breaks the session outright: the churn exhausts the
// local stream budget and createBidirectionalStream() starts throwing
// ERR_QUIC_OPEN_STREAM_FAILED.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, makePayload } = await import('../common/quic.mjs');

// The payload has to be big enough that the server still has plenty in flight
// when the client walks away, which is what creates the stale frames.
const kPayloadSize = 256 * 1024;
const kStreams = 12;

const payload = makePayload(kPayloadSize, 13);

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall((stream) => {
    // The client destroys these early, so the write is cut short by
    // STOP_SENDING. That is the scenario under test, not a failure.
    stream.onerror = () => {};
    stream.setBody(payload);
  }, kStreams);
}));

const clientSession = await connect(serverEndpoint.address);

// The client never expects an incoming stream: it opens every stream itself
// and the server opens none. Any onstream event here is a resurrected local
// stream being mistaken for a peer-initiated one.
clientSession.onstream = mustNotCall(
  'client must not receive onstream for its own destroyed streams');

await clientSession.opened;

for (let i = 0; i < kStreams; i++) {
  const stream = await clientSession.createBidirectionalStream({
    body: new Uint8Array(8),
  });

  // Read a little, then abandon the rest and destroy. Breaking out of the
  // loop leaves the server mid-send, so frames keep arriving after destroy.
  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream) break;

  stream.destroy();
}

// Exactly one locally-opened stream per iteration. Before the fix this
// counted in the hundreds, because every stale frame created a new stream.
assert.strictEqual(Number(clientSession.stats.bidiOutStreamCount), kStreams);

await clientSession.close();
await serverEndpoint.close();

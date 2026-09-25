// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: large simultaneous transfers in both directions on one stream,
// through constrained windows in both directions.
//
// Each direction of a bidirectional stream has its own independent flow
// control window, governed by different transport parameters:
//
//   client -> server  is limited by the server's
//                     initialMaxStreamDataBidiRemote
//   server -> client  is limited by the client's
//                     initialMaxStreamDataBidiLocal
//
// Running both directions at volume at the same time means credit is being
// consumed and returned in both directions concurrently on the same stream.
// A bug that credits the wrong direction, or that lets one direction's
// accounting interfere with the other's, shows up here but not in the
// one-direction-at-a-time tests.
//
// This exercises the raw QUIC application: the test helpers negotiate the
// 'quic-test' ALPN, not HTTP/3. See test-quic-h3-flow-control-volume.mjs for
// the HTTP/3 equivalent.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, makePayload, hashBytes } =
  await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const kTotal = 4 * 1024 * 1024;         // per direction
const kStreamWindow = 16 * 1024;
const kConnWindow = 32 * 1024;

assert.ok(kTotal / kConnWindow >= 100,
          'payload must require >=100 connection window refills');

// Distinct payloads per direction so a direction mixup is detectable.
const toServer = makePayload(kTotal, 11);
const toClient = makePayload(kTotal, 22);
const toServerHash = hashBytes(toServer);
const toClientHash = hashBytes(toClient);

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Start sending before reading, so both directions are in flight at
    // once. If the server read to completion first, the two directions
    // would be sequential and the test would prove much less.
    stream.setBody(toClient);

    const received = await bytes(stream);
    assert.strictEqual(received.byteLength, kTotal);
    // Must be the client->server payload, not an echo of our own.
    assert.strictEqual(hashBytes(received), toServerHash);

    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  transportParams: {
    // Limits client -> server.
    initialMaxStreamDataBidiRemote: kStreamWindow,
    initialMaxData: kConnWindow,
  },
  maxStreamWindow: kStreamWindow,
  maxWindow: kConnWindow,
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: {
    // Limits server -> client.
    initialMaxStreamDataBidiLocal: kStreamWindow,
    initialMaxData: kConnWindow,
  },
  maxStreamWindow: kStreamWindow,
  maxWindow: kConnWindow,
});
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();
stream.setBody(toServer);

// Read the server's payload while our own body is still being sent.
const received = await bytes(stream);
assert.strictEqual(received.byteLength, kTotal);
// Must be the server->client payload, not an echo of our own.
assert.strictEqual(hashBytes(received), toClientHash);

// Sanity: the two directions carried genuinely different data, so the
// assertions above could not both be satisfied by one payload echoed back.
assert.notStrictEqual(toServerHash, toClientHash);

await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
await serverEndpoint.close();

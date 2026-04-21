// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream stats fields.
// Verify that stream stats are populated with correct types and
// that bytesReceived/bytesSent reflect actual data transfer.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const payload = encoder.encode('stream stats test data');
const payloadLength = payload.byteLength;
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const data = await bytes(stream);
    strictEqual(data.byteLength, payloadLength);

    // Stream stats should reflect received bytes.
    strictEqual(stream.stats.bytesReceived, BigInt(payloadLength));
    strictEqual(typeof stream.stats.createdAt, 'bigint');
    strictEqual(typeof stream.stats.receivedAt, 'bigint');

    // Send response.
    stream.setBody(encoder.encode('response'));
    await stream.closed;

    // After close, bytesSent should reflect response.
    strictEqual(stream.stats.bytesSent, BigInt('response'.length));

    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: payload,
});

// Stats should have correct types before transfer completes.
strictEqual(typeof stream.stats.createdAt, 'bigint');
strictEqual(typeof stream.stats.bytesReceived, 'bigint');
strictEqual(typeof stream.stats.bytesSent, 'bigint');
strictEqual(typeof stream.stats.maxOffset, 'bigint');

// Verify toJSON works.
const json = stream.stats.toJSON();
ok(json);
strictEqual(typeof json.createdAt, 'string');

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await stream.closed;
await serverDone.promise;

// After transfer, bytesSent should reflect the payload.
strictEqual(stream.stats.bytesSent, BigInt(payloadLength));
ok(stream.stats.bytesReceived > 0n);

await clientSession.closed;
await serverEndpoint.close();

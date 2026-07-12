// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: session.applicationOptions
// Verifies that applicationOptions is available after ALPN negotiation
// completes (i.e., once the application has been selected), returns a
// null-prototype object, and reflects the configured values.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const customAppOptions = {
  maxHeaderPairs: 50n,
  maxHeaderLength: 8192n,
  maxFieldSectionSize: 16384n,
  qpackMaxDTableCapacity: 2048n,
  qpackEncoderMaxDTableCapacity: 2048n,
  qpackBlockedStreams: 50n,
  enableConnectProtocol: false,
  enableDatagrams: false,
};

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // After the stream arrives, the handshake and ALPN negotiation are
    // complete, so applicationOptions should be available.
    const opts = serverSession.applicationOptions;

    assert.ok(opts != null, 'server applicationOptions should be available after handshake');
    assert.strictEqual(typeof opts, 'object');
    assert.strictEqual(Object.getPrototypeOf(opts), null);

    // Verify configured values are reflected.
    assert.strictEqual(opts.maxHeaderPairs, BigInt(customAppOptions.maxHeaderPairs));
    assert.strictEqual(opts.maxHeaderLength, BigInt(customAppOptions.maxHeaderLength));
    assert.strictEqual(opts.maxFieldSectionSize,
                       BigInt(customAppOptions.maxFieldSectionSize));
    assert.strictEqual(opts.qpackMaxDtableCapacity,
                       BigInt(customAppOptions.qpackMaxDTableCapacity));
    assert.strictEqual(opts.qpackEncoderMaxDtableCapacity,
                       BigInt(customAppOptions.qpackEncoderMaxDTableCapacity));
    assert.strictEqual(opts.qpackBlockedStreams,
                       BigInt(customAppOptions.qpackBlockedStreams));
    assert.strictEqual(opts.enableConnectProtocol,
                       customAppOptions.enableConnectProtocol);
    assert.strictEqual(opts.enableDatagrams, customAppOptions.enableDatagrams);

    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  application: customAppOptions,
});

const clientSession = await connect(serverEndpoint.address, {
  application: customAppOptions,
});
await clientSession.opened;

// After opened, ALPN negotiation is complete and applicationOptions
// should be available on the client session.
const clientOpts = clientSession.applicationOptions;
assert.ok(clientOpts != null, 'client applicationOptions should be available after handshake');
assert.strictEqual(typeof clientOpts, 'object');
assert.strictEqual(Object.getPrototypeOf(clientOpts), null);

// Verify configured values on the client side.
assert.strictEqual(clientOpts.maxHeaderPairs, BigInt(customAppOptions.maxHeaderPairs));
assert.strictEqual(clientOpts.maxHeaderLength, BigInt(customAppOptions.maxHeaderLength));
assert.strictEqual(clientOpts.maxFieldSectionSize,
                   customAppOptions.maxFieldSectionSize);
assert.strictEqual(clientOpts.qpackMaxDtableCapacity,
                   customAppOptions.qpackMaxDTableCapacity);
assert.strictEqual(clientOpts.qpackEncoderMaxDtableCapacity,
                   customAppOptions.qpackEncoderMaxDTableCapacity);
assert.strictEqual(clientOpts.qpackBlockedStreams,
                   customAppOptions.qpackBlockedStreams);
assert.strictEqual(clientOpts.enableConnectProtocol,
                   customAppOptions.enableConnectProtocol);
assert.strictEqual(clientOpts.enableDatagrams, customAppOptions.enableDatagrams);

// Exchange data to let the server side run its assertions.
const stream = await clientSession.createBidirectionalStream();
stream.writer.endSync();

// eslint-disable-next-line no-unused-vars
for await (const _ of stream) { /* drain */ }
await Promise.all([stream.closed, serverDone.promise]);

// After close, applicationOptions should return null.
await clientSession.close();
assert.strictEqual(clientSession.applicationOptions, null);

await serverEndpoint.close();

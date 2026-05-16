// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: session.applicationOptions
// Verifies that applicationOptions is available after ALPN negotiation
// completes (i.e., once the application has been selected), returns a
// null-prototype object, and reflects the configured values.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

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

    ok(opts != null, 'server applicationOptions should be available after handshake');
    strictEqual(typeof opts, 'object');
    strictEqual(Object.getPrototypeOf(opts), null);

    // Verify configured values are reflected.
    strictEqual(opts.maxHeaderPairs, BigInt(customAppOptions.maxHeaderPairs));
    strictEqual(opts.maxHeaderLength, BigInt(customAppOptions.maxHeaderLength));
    strictEqual(opts.maxFieldSectionSize,
                BigInt(customAppOptions.maxFieldSectionSize));
    strictEqual(opts.qpackMaxDtableCapacity,
                BigInt(customAppOptions.qpackMaxDTableCapacity));
    strictEqual(opts.qpackEncoderMaxDtableCapacity,
                BigInt(customAppOptions.qpackEncoderMaxDTableCapacity));
    strictEqual(opts.qpackBlockedStreams,
                BigInt(customAppOptions.qpackBlockedStreams));
    strictEqual(opts.enableConnectProtocol,
                customAppOptions.enableConnectProtocol);
    strictEqual(opts.enableDatagrams, customAppOptions.enableDatagrams);

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
ok(clientOpts != null, 'client applicationOptions should be available after handshake');
strictEqual(typeof clientOpts, 'object');
strictEqual(Object.getPrototypeOf(clientOpts), null);

// Verify configured values on the client side.
strictEqual(clientOpts.maxHeaderPairs, BigInt(customAppOptions.maxHeaderPairs));
strictEqual(clientOpts.maxHeaderLength, BigInt(customAppOptions.maxHeaderLength));
strictEqual(clientOpts.maxFieldSectionSize,
            customAppOptions.maxFieldSectionSize);
strictEqual(clientOpts.qpackMaxDtableCapacity,
            customAppOptions.qpackMaxDTableCapacity);
strictEqual(clientOpts.qpackEncoderMaxDtableCapacity,
            customAppOptions.qpackEncoderMaxDTableCapacity);
strictEqual(clientOpts.qpackBlockedStreams,
            customAppOptions.qpackBlockedStreams);
strictEqual(clientOpts.enableConnectProtocol,
            customAppOptions.enableConnectProtocol);
strictEqual(clientOpts.enableDatagrams, customAppOptions.enableDatagrams);

// Exchange data to let the server side run its assertions.
const stream = await clientSession.createBidirectionalStream();
stream.writer.endSync();

// eslint-disable-next-line no-unused-vars
for await (const _ of stream) { /* drain */ }
await Promise.all([stream.closed, serverDone.promise]);

// After close, applicationOptions should return null.
await clientSession.close();
strictEqual(clientSession.applicationOptions, null);

await serverEndpoint.close();

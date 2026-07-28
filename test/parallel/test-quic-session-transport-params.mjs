// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: session.localTransportParams and session.remoteTransportParams
// Verifies that local transport params are available immediately after
// session creation, remote transport params are available after the
// handshake completes, and values match what was configured.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverTransportParams = {
  initialMaxStreamsBidi: 200,
  initialMaxData: 2 * 1024 * 1024,
};

const clientTransportParams = {
  initialMaxStreamsBidi: 500,
  initialMaxData: 8 * 1024 * 1024,
};

const serverDone = Promise.withResolvers();
let serverLocalParams;
let serverRemoteParams;

const serverEndpoint = await listen(mustCall((serverSession) => {
  // localTransportParams should be available immediately.
  serverLocalParams = serverSession.localTransportParams;
  assert.ok(serverLocalParams != null, 'server localTransportParams should be available immediately');
  assert.strictEqual(typeof serverLocalParams, 'object');
  assert.strictEqual(Object.getPrototypeOf(serverLocalParams), null);

  // Verify server's configured values are reflected.
  assert.strictEqual(serverLocalParams.initialMaxStreamsBidi,
                     BigInt(serverTransportParams.initialMaxStreamsBidi));
  assert.strictEqual(serverLocalParams.initialMaxData,
                     BigInt(serverTransportParams.initialMaxData));

  // Verify defaults are present and reasonable.
  assert.ok(serverLocalParams.activeConnectionIDLimit > 0n,
            'activeConnectionIDLimit should be positive');
  assert.ok(serverLocalParams.ackDelayExponent > 0n,
            'ackDelayExponent should be positive');
  assert.ok(serverLocalParams.maxAckDelay > 0n,
            'maxAckDelay should be positive');
  assert.strictEqual(serverLocalParams.disableActiveMigration, false);

  serverSession.onstream = mustCall(async (stream) => {
    // After the stream arrives, the handshake is complete and
    // remoteTransportParams should be available.
    serverRemoteParams = serverSession.remoteTransportParams;
    assert.ok(serverRemoteParams != null,
              'server remoteTransportParams should be available after handshake');
    assert.strictEqual(typeof serverRemoteParams, 'object');
    assert.strictEqual(Object.getPrototypeOf(serverRemoteParams), null);

    // Remote params should reflect the client's configured values.
    assert.strictEqual(serverRemoteParams.initialMaxStreamsBidi,
                       BigInt(clientTransportParams.initialMaxStreamsBidi));
    assert.strictEqual(serverRemoteParams.initialMaxData,
                       BigInt(clientTransportParams.initialMaxData));

    // CID fields should be present.
    assert.strictEqual(typeof serverRemoteParams.initialSCID, 'string');
    assert.ok(serverRemoteParams.initialSCID.length > 0,
              'remote initialSCID should be non-empty');

    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}), {
  transportParams: serverTransportParams,
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: clientTransportParams,
});
await clientSession.opened;

// After opened, the handshake is complete. Both local and remote
// transport params should be available on the client session.
const clientLocalParams = clientSession.localTransportParams;
assert.ok(clientLocalParams != null, 'client localTransportParams should be available');
assert.strictEqual(typeof clientLocalParams, 'object');
assert.strictEqual(Object.getPrototypeOf(clientLocalParams), null);

// Verify client's configured values.
assert.strictEqual(clientLocalParams.initialMaxStreamsBidi,
                   BigInt(clientTransportParams.initialMaxStreamsBidi));
assert.strictEqual(clientLocalParams.initialMaxData,
                   BigInt(clientTransportParams.initialMaxData));

const clientRemoteParams = clientSession.remoteTransportParams;
assert.ok(clientRemoteParams != null,
          'client remoteTransportParams should be available after handshake');
assert.strictEqual(typeof clientRemoteParams, 'object');
assert.strictEqual(Object.getPrototypeOf(clientRemoteParams), null);

// Remote params should reflect the server's configured values.
assert.strictEqual(clientRemoteParams.initialMaxStreamsBidi,
                   BigInt(serverTransportParams.initialMaxStreamsBidi));
assert.strictEqual(clientRemoteParams.initialMaxData,
                   BigInt(serverTransportParams.initialMaxData));

// CID fields should be present on the client's view of server params.
assert.strictEqual(typeof clientRemoteParams.initialSCID, 'string');
assert.ok(clientRemoteParams.initialSCID.length > 0,
          'remote initialSCID should be non-empty');

// Cross-validation: server's remote matches client's local and vice versa.
const stream = await clientSession.createBidirectionalStream();
stream.writer.endSync();

// eslint-disable-next-line no-unused-vars
for await (const _ of stream) { /* drain */ }
await stream.closed;
await serverDone.promise;

// Cross-validate after both sides have captured params.
assert.strictEqual(serverRemoteParams.initialMaxStreamsBidi,
                   clientLocalParams.initialMaxStreamsBidi);
assert.strictEqual(serverRemoteParams.initialMaxData,
                   clientLocalParams.initialMaxData);
assert.strictEqual(clientRemoteParams.initialMaxStreamsBidi,
                   serverLocalParams.initialMaxStreamsBidi);
assert.strictEqual(clientRemoteParams.initialMaxData,
                   serverLocalParams.initialMaxData);

await clientSession.close();
await serverEndpoint.close();

// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: session.localTransportParams and session.remoteTransportParams
// Verifies that local transport params are available immediately after
// session creation, remote transport params are available after the
// handshake completes, and values match what was configured.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

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
  ok(serverLocalParams != null, 'server localTransportParams should be available immediately');
  strictEqual(typeof serverLocalParams, 'object');
  strictEqual(Object.getPrototypeOf(serverLocalParams), null);

  // Verify server's configured values are reflected.
  strictEqual(serverLocalParams.initialMaxStreamsBidi,
              BigInt(serverTransportParams.initialMaxStreamsBidi));
  strictEqual(serverLocalParams.initialMaxData,
              BigInt(serverTransportParams.initialMaxData));

  // Verify defaults are present and reasonable.
  ok(serverLocalParams.activeConnectionIDLimit > 0n,
     'activeConnectionIDLimit should be positive');
  ok(serverLocalParams.ackDelayExponent > 0n,
     'ackDelayExponent should be positive');
  ok(serverLocalParams.maxAckDelay > 0n,
     'maxAckDelay should be positive');
  strictEqual(serverLocalParams.disableActiveMigration, false);

  serverSession.onstream = mustCall(async (stream) => {
    // After the stream arrives, the handshake is complete and
    // remoteTransportParams should be available.
    serverRemoteParams = serverSession.remoteTransportParams;
    ok(serverRemoteParams != null,
       'server remoteTransportParams should be available after handshake');
    strictEqual(typeof serverRemoteParams, 'object');
    strictEqual(Object.getPrototypeOf(serverRemoteParams), null);

    // Remote params should reflect the client's configured values.
    strictEqual(serverRemoteParams.initialMaxStreamsBidi,
                BigInt(clientTransportParams.initialMaxStreamsBidi));
    strictEqual(serverRemoteParams.initialMaxData,
                BigInt(clientTransportParams.initialMaxData));

    // CID fields should be present.
    strictEqual(typeof serverRemoteParams.initialSCID, 'string');
    ok(serverRemoteParams.initialSCID.length > 0,
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
ok(clientLocalParams != null, 'client localTransportParams should be available');
strictEqual(typeof clientLocalParams, 'object');
strictEqual(Object.getPrototypeOf(clientLocalParams), null);

// Verify client's configured values.
strictEqual(clientLocalParams.initialMaxStreamsBidi,
            BigInt(clientTransportParams.initialMaxStreamsBidi));
strictEqual(clientLocalParams.initialMaxData,
            BigInt(clientTransportParams.initialMaxData));

const clientRemoteParams = clientSession.remoteTransportParams;
ok(clientRemoteParams != null,
   'client remoteTransportParams should be available after handshake');
strictEqual(typeof clientRemoteParams, 'object');
strictEqual(Object.getPrototypeOf(clientRemoteParams), null);

// Remote params should reflect the server's configured values.
strictEqual(clientRemoteParams.initialMaxStreamsBidi,
            BigInt(serverTransportParams.initialMaxStreamsBidi));
strictEqual(clientRemoteParams.initialMaxData,
            BigInt(serverTransportParams.initialMaxData));

// CID fields should be present on the client's view of server params.
strictEqual(typeof clientRemoteParams.initialSCID, 'string');
ok(clientRemoteParams.initialSCID.length > 0,
   'remote initialSCID should be non-empty');

// Cross-validation: server's remote matches client's local and vice versa.
const stream = await clientSession.createBidirectionalStream();
stream.writer.endSync();

// eslint-disable-next-line no-unused-vars
for await (const _ of stream) { /* drain */ }
await stream.closed;
await serverDone.promise;

// Cross-validate after both sides have captured params.
strictEqual(serverRemoteParams.initialMaxStreamsBidi,
            clientLocalParams.initialMaxStreamsBidi);
strictEqual(serverRemoteParams.initialMaxData,
            clientLocalParams.initialMaxData);
strictEqual(clientRemoteParams.initialMaxStreamsBidi,
            serverLocalParams.initialMaxStreamsBidi);
strictEqual(clientRemoteParams.initialMaxData,
            serverLocalParams.initialMaxData);

await clientSession.close();
await serverEndpoint.close();

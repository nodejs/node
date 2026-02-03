// Flags: --expose-internals --experimental-quic --no-warnings
import { hasQuic, skip } from '../common/index.mjs';
import { inspect } from 'node:util';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { QuicEndpoint } = await import('node:quic');
const {
  QuicSessionState,
  QuicStreamState,
} = (await import('internal/quic/state')).default;
const {
  QuicSessionStats,
  QuicStreamStats,
} = (await import('internal/quic/stats')).default;
const {
  kFinishClose,
  kPrivateConstructor,
} = (await import('internal/quic/symbols')).default;
const {
  getQuicEndpointState,
} = (await import('internal/quic/quic')).default;

{
  const endpoint = new QuicEndpoint();
  const state = getQuicEndpointState(endpoint);

  assert.strictEqual(state.isBound, false);
  assert.strictEqual(state.isReceiving, false);
  assert.strictEqual(state.isListening, false);
  assert.strictEqual(state.isClosing, false);
  assert.strictEqual(state.isBusy, false);
  assert.strictEqual(state.pendingCallbacks, 0n);

  assert.deepStrictEqual(JSON.parse(JSON.stringify(state)), {
    isBound: false,
    isReceiving: false,
    isListening: false,
    isClosing: false,
    isBusy: false,
    pendingCallbacks: '0',
  });

  endpoint.busy = true;
  assert.strictEqual(state.isBusy, true);
  endpoint.busy = false;
  assert.strictEqual(state.isBusy, false);
  assert.strictEqual(typeof inspect(state), 'string');
}

{
  // It is not bound after close.
  const endpoint = new QuicEndpoint();
  const state = getQuicEndpointState(endpoint);
  state[kFinishClose]();
  assert.strictEqual(state.isBound, undefined);
}

{
  // State constructor argument is ArrayBuffer
  const endpoint = new QuicEndpoint();
  const state = getQuicEndpointState(endpoint);
  const StateCons = state.constructor;
  assert.throws(() => new StateCons(kPrivateConstructor, 1), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
}

{
  // Endpoint stats are readable and have expected properties
  const endpoint = new QuicEndpoint();

  assert.strictEqual(typeof endpoint.stats.isConnected, 'boolean');
  assert.strictEqual(typeof endpoint.stats.createdAt, 'bigint');
  assert.strictEqual(typeof endpoint.stats.destroyedAt, 'bigint');
  assert.strictEqual(typeof endpoint.stats.bytesReceived, 'bigint');
  assert.strictEqual(typeof endpoint.stats.bytesSent, 'bigint');
  assert.strictEqual(typeof endpoint.stats.packetsReceived, 'bigint');
  assert.strictEqual(typeof endpoint.stats.packetsSent, 'bigint');
  assert.strictEqual(typeof endpoint.stats.serverSessions, 'bigint');
  assert.strictEqual(typeof endpoint.stats.clientSessions, 'bigint');
  assert.strictEqual(typeof endpoint.stats.serverBusyCount, 'bigint');
  assert.strictEqual(typeof endpoint.stats.retryCount, 'bigint');
  assert.strictEqual(typeof endpoint.stats.versionNegotiationCount, 'bigint');
  assert.strictEqual(typeof endpoint.stats.statelessResetCount, 'bigint');
  assert.strictEqual(typeof endpoint.stats.immediateCloseCount, 'bigint');

  assert.deepStrictEqual(Object.keys(endpoint.stats.toJSON()), [
    'connected',
    'createdAt',
    'destroyedAt',
    'bytesReceived',
    'bytesSent',
    'packetsReceived',
    'packetsSent',
    'serverSessions',
    'clientSessions',
    'serverBusyCount',
    'retryCount',
    'versionNegotiationCount',
    'statelessResetCount',
    'immediateCloseCount',
  ]);
  assert.strictEqual(typeof inspect(endpoint.stats), 'string');
}

{
  // Stats are still readable after close
  const endpoint = new QuicEndpoint();
  assert.strictEqual(typeof endpoint.stats.toJSON(), 'object');
  endpoint.stats[kFinishClose]();
  assert.strictEqual(endpoint.stats.isConnected, false);
  assert.strictEqual(typeof endpoint.stats.destroyedAt, 'bigint');
  assert.strictEqual(typeof endpoint.stats.toJSON(), 'object');
}

{
  // Stats constructor argument is ArrayBuffer
  const endpoint = new QuicEndpoint();
  const StatsCons = endpoint.stats.constructor;
  assert.throws(() => new StatsCons(kPrivateConstructor, 1), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

// TODO(@jasnell): The following tests are largely incomplete.
// This is largely here to boost the code coverage numbers
// temporarily while the rest of the functionality is being
// implemented.
const streamState = new QuicStreamState(kPrivateConstructor, new ArrayBuffer(1024));
const sessionState = new QuicSessionState(kPrivateConstructor, new ArrayBuffer(1024));

assert.strictEqual(streamState.pending, false);
assert.strictEqual(streamState.finSent, false);
assert.strictEqual(streamState.finReceived, false);
assert.strictEqual(streamState.readEnded, false);
assert.strictEqual(streamState.writeEnded, false);
assert.strictEqual(streamState.paused, false);
assert.strictEqual(streamState.reset, false);
assert.strictEqual(streamState.hasReader, false);
assert.strictEqual(streamState.wantsBlock, false);
assert.strictEqual(streamState.wantsReset, false);

assert.strictEqual(sessionState.hasPathValidationListener, false);
assert.strictEqual(sessionState.hasVersionNegotiationListener, false);
assert.strictEqual(sessionState.hasDatagramListener, false);
assert.strictEqual(sessionState.hasSessionTicketListener, false);
assert.strictEqual(sessionState.isClosing, false);
assert.strictEqual(sessionState.isGracefulClose, false);
assert.strictEqual(sessionState.isSilentClose, false);
assert.strictEqual(sessionState.isStatelessReset, false);
assert.strictEqual(sessionState.isHandshakeCompleted, false);
assert.strictEqual(sessionState.isHandshakeConfirmed, false);
assert.strictEqual(sessionState.isStreamOpenAllowed, false);
assert.strictEqual(sessionState.isPrioritySupported, false);
assert.strictEqual(sessionState.isWrapped, false);
assert.strictEqual(sessionState.lastDatagramId, 0n);

assert.strictEqual(typeof streamState.toJSON(), 'object');
assert.strictEqual(typeof sessionState.toJSON(), 'object');
assert.strictEqual(typeof inspect(streamState), 'string');
assert.strictEqual(typeof inspect(sessionState), 'string');

const streamStats = new QuicStreamStats(kPrivateConstructor, new ArrayBuffer(1024));
const sessionStats = new QuicSessionStats(kPrivateConstructor, new ArrayBuffer(1024));
assert.strictEqual(streamStats.createdAt, 0n);
assert.strictEqual(streamStats.openedAt, 0n);
assert.strictEqual(streamStats.receivedAt, 0n);
assert.strictEqual(streamStats.ackedAt, 0n);
assert.strictEqual(streamStats.destroyedAt, 0n);
assert.strictEqual(streamStats.bytesReceived, 0n);
assert.strictEqual(streamStats.bytesSent, 0n);
assert.strictEqual(streamStats.maxOffset, 0n);
assert.strictEqual(streamStats.maxOffsetAcknowledged, 0n);
assert.strictEqual(streamStats.maxOffsetReceived, 0n);
assert.strictEqual(streamStats.finalSize, 0n);
assert.strictEqual(typeof streamStats.toJSON(), 'object');
assert.strictEqual(typeof inspect(streamStats), 'string');
streamStats[kFinishClose]();

assert.strictEqual(typeof sessionStats.createdAt, 'bigint');
assert.strictEqual(typeof sessionStats.closingAt, 'bigint');
assert.strictEqual(typeof sessionStats.handshakeCompletedAt, 'bigint');
assert.strictEqual(typeof sessionStats.handshakeConfirmedAt, 'bigint');
assert.strictEqual(typeof sessionStats.bytesReceived, 'bigint');
assert.strictEqual(typeof sessionStats.bytesSent, 'bigint');
assert.strictEqual(typeof sessionStats.bidiInStreamCount, 'bigint');
assert.strictEqual(typeof sessionStats.bidiOutStreamCount, 'bigint');
assert.strictEqual(typeof sessionStats.uniInStreamCount, 'bigint');
assert.strictEqual(typeof sessionStats.uniOutStreamCount, 'bigint');
assert.strictEqual(typeof sessionStats.maxBytesInFlights, 'bigint');
assert.strictEqual(typeof sessionStats.bytesInFlight, 'bigint');
assert.strictEqual(typeof sessionStats.blockCount, 'bigint');
assert.strictEqual(typeof sessionStats.cwnd, 'bigint');
assert.strictEqual(typeof sessionStats.latestRtt, 'bigint');
assert.strictEqual(typeof sessionStats.minRtt, 'bigint');
assert.strictEqual(typeof sessionStats.rttVar, 'bigint');
assert.strictEqual(typeof sessionStats.smoothedRtt, 'bigint');
assert.strictEqual(typeof sessionStats.ssthresh, 'bigint');
assert.strictEqual(typeof sessionStats.datagramsReceived, 'bigint');
assert.strictEqual(typeof sessionStats.datagramsSent, 'bigint');
assert.strictEqual(typeof sessionStats.datagramsAcknowledged, 'bigint');
assert.strictEqual(typeof sessionStats.datagramsLost, 'bigint');
assert.strictEqual(typeof sessionStats.toJSON(), 'object');
assert.strictEqual(typeof inspect(sessionStats), 'string');
streamStats[kFinishClose]();

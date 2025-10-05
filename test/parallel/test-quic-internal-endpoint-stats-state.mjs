// Flags: --expose-internals --experimental-quic --no-warnings
import { hasQuic, skip } from '../common/index.mjs';
import { inspect } from 'node:util';
import {
  deepStrictEqual,
  strictEqual,
  throws,
} from 'node:assert';

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

  strictEqual(state.isBound, false);
  strictEqual(state.isReceiving, false);
  strictEqual(state.isListening, false);
  strictEqual(state.isClosing, false);
  strictEqual(state.isBusy, false);
  strictEqual(state.pendingCallbacks, 0n);

  deepStrictEqual(JSON.parse(JSON.stringify(state)), {
    isBound: false,
    isReceiving: false,
    isListening: false,
    isClosing: false,
    isBusy: false,
    pendingCallbacks: '0',
  });

  endpoint.busy = true;
  strictEqual(state.isBusy, true);
  endpoint.busy = false;
  strictEqual(state.isBusy, false);
  strictEqual(typeof inspect(state), 'string');
}

{
  // It is not bound after close.
  const endpoint = new QuicEndpoint();
  const state = getQuicEndpointState(endpoint);
  state[kFinishClose]();
  strictEqual(state.isBound, undefined);
}

{
  // State constructor argument is ArrayBuffer
  const endpoint = new QuicEndpoint();
  const state = getQuicEndpointState(endpoint);
  const StateCons = state.constructor;
  throws(() => new StateCons(kPrivateConstructor, 1), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
}

{
  // Endpoint stats are readable and have expected properties
  const endpoint = new QuicEndpoint();

  strictEqual(typeof endpoint.stats.isConnected, 'boolean');
  strictEqual(typeof endpoint.stats.createdAt, 'bigint');
  strictEqual(typeof endpoint.stats.destroyedAt, 'bigint');
  strictEqual(typeof endpoint.stats.bytesReceived, 'bigint');
  strictEqual(typeof endpoint.stats.bytesSent, 'bigint');
  strictEqual(typeof endpoint.stats.packetsReceived, 'bigint');
  strictEqual(typeof endpoint.stats.packetsSent, 'bigint');
  strictEqual(typeof endpoint.stats.serverSessions, 'bigint');
  strictEqual(typeof endpoint.stats.clientSessions, 'bigint');
  strictEqual(typeof endpoint.stats.serverBusyCount, 'bigint');
  strictEqual(typeof endpoint.stats.retryCount, 'bigint');
  strictEqual(typeof endpoint.stats.versionNegotiationCount, 'bigint');
  strictEqual(typeof endpoint.stats.statelessResetCount, 'bigint');
  strictEqual(typeof endpoint.stats.immediateCloseCount, 'bigint');

  deepStrictEqual(Object.keys(endpoint.stats.toJSON()), [
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
  strictEqual(typeof inspect(endpoint.stats), 'string');
}

{
  // Stats are still readable after close
  const endpoint = new QuicEndpoint();
  strictEqual(typeof endpoint.stats.toJSON(), 'object');
  endpoint.stats[kFinishClose]();
  strictEqual(endpoint.stats.isConnected, false);
  strictEqual(typeof endpoint.stats.destroyedAt, 'bigint');
  strictEqual(typeof endpoint.stats.toJSON(), 'object');
}

{
  // Stats constructor argument is ArrayBuffer
  const endpoint = new QuicEndpoint();
  const StatsCons = endpoint.stats.constructor;
  throws(() => new StatsCons(kPrivateConstructor, 1), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

// TODO(@jasnell): The following tests are largely incomplete.
// This is largely here to boost the code coverage numbers
// temporarily while the rest of the functionality is being
// implemented.
const streamState = new QuicStreamState(kPrivateConstructor, new ArrayBuffer(1024));
const sessionState = new QuicSessionState(kPrivateConstructor, new ArrayBuffer(1024));

strictEqual(streamState.pending, false);
strictEqual(streamState.finSent, false);
strictEqual(streamState.finReceived, false);
strictEqual(streamState.readEnded, false);
strictEqual(streamState.writeEnded, false);
strictEqual(streamState.paused, false);
strictEqual(streamState.reset, false);
strictEqual(streamState.hasReader, false);
strictEqual(streamState.wantsBlock, false);
strictEqual(streamState.wantsReset, false);

strictEqual(sessionState.hasPathValidationListener, false);
strictEqual(sessionState.hasVersionNegotiationListener, false);
strictEqual(sessionState.hasDatagramListener, false);
strictEqual(sessionState.hasSessionTicketListener, false);
strictEqual(sessionState.isClosing, false);
strictEqual(sessionState.isGracefulClose, false);
strictEqual(sessionState.isSilentClose, false);
strictEqual(sessionState.isStatelessReset, false);
strictEqual(sessionState.isHandshakeCompleted, false);
strictEqual(sessionState.isHandshakeConfirmed, false);
strictEqual(sessionState.isStreamOpenAllowed, false);
strictEqual(sessionState.isPrioritySupported, false);
strictEqual(sessionState.isWrapped, false);
strictEqual(sessionState.lastDatagramId, 0n);

strictEqual(typeof streamState.toJSON(), 'object');
strictEqual(typeof sessionState.toJSON(), 'object');
strictEqual(typeof inspect(streamState), 'string');
strictEqual(typeof inspect(sessionState), 'string');

const streamStats = new QuicStreamStats(kPrivateConstructor, new ArrayBuffer(1024));
const sessionStats = new QuicSessionStats(kPrivateConstructor, new ArrayBuffer(1024));
strictEqual(streamStats.createdAt, 0n);
strictEqual(streamStats.openedAt, 0n);
strictEqual(streamStats.receivedAt, 0n);
strictEqual(streamStats.ackedAt, 0n);
strictEqual(streamStats.destroyedAt, 0n);
strictEqual(streamStats.bytesReceived, 0n);
strictEqual(streamStats.bytesSent, 0n);
strictEqual(streamStats.maxOffset, 0n);
strictEqual(streamStats.maxOffsetAcknowledged, 0n);
strictEqual(streamStats.maxOffsetReceived, 0n);
strictEqual(streamStats.finalSize, 0n);
strictEqual(typeof streamStats.toJSON(), 'object');
strictEqual(typeof inspect(streamStats), 'string');
streamStats[kFinishClose]();

strictEqual(typeof sessionStats.createdAt, 'bigint');
strictEqual(typeof sessionStats.closingAt, 'bigint');
strictEqual(typeof sessionStats.handshakeCompletedAt, 'bigint');
strictEqual(typeof sessionStats.handshakeConfirmedAt, 'bigint');
strictEqual(typeof sessionStats.bytesReceived, 'bigint');
strictEqual(typeof sessionStats.bytesSent, 'bigint');
strictEqual(typeof sessionStats.bidiInStreamCount, 'bigint');
strictEqual(typeof sessionStats.bidiOutStreamCount, 'bigint');
strictEqual(typeof sessionStats.uniInStreamCount, 'bigint');
strictEqual(typeof sessionStats.uniOutStreamCount, 'bigint');
strictEqual(typeof sessionStats.maxBytesInFlights, 'bigint');
strictEqual(typeof sessionStats.bytesInFlight, 'bigint');
strictEqual(typeof sessionStats.blockCount, 'bigint');
strictEqual(typeof sessionStats.cwnd, 'bigint');
strictEqual(typeof sessionStats.latestRtt, 'bigint');
strictEqual(typeof sessionStats.minRtt, 'bigint');
strictEqual(typeof sessionStats.rttVar, 'bigint');
strictEqual(typeof sessionStats.smoothedRtt, 'bigint');
strictEqual(typeof sessionStats.ssthresh, 'bigint');
strictEqual(typeof sessionStats.datagramsReceived, 'bigint');
strictEqual(typeof sessionStats.datagramsSent, 'bigint');
strictEqual(typeof sessionStats.datagramsAcknowledged, 'bigint');
strictEqual(typeof sessionStats.datagramsLost, 'bigint');
strictEqual(typeof sessionStats.toJSON(), 'object');
strictEqual(typeof inspect(sessionStats), 'string');
streamStats[kFinishClose]();

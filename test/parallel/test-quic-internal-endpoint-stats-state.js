// Flags: --expose-internals
'use strict';

const { hasQuic } = require('../common');

const {
  describe,
  it,
} = require('node:test');

describe('quic internal endpoint stats and state', { skip: !hasQuic }, () => {
  const {
    QuicEndpoint,
  } = require('internal/quic/quic');

  const {
    QuicSessionState,
    QuicStreamState,
  } = require('internal/quic/state');

  const {
    QuicSessionStats,
    QuicStreamStats,
  } = require('internal/quic/stats');

  const {
    kFinishClose,
    kPrivateConstructor,
    kState,
  } = require('internal/quic/symbols');

  const {
    inspect,
  } = require('util');

  const assert = require('node:assert');

  it('endpoint state', () => {
    const endpoint = new QuicEndpoint();

    assert.strictEqual(endpoint[kState].isBound, false);
    assert.strictEqual(endpoint[kState].isReceiving, false);
    assert.strictEqual(endpoint[kState].isListening, false);
    assert.strictEqual(endpoint[kState].isClosing, false);
    assert.strictEqual(endpoint[kState].isBusy, false);
    assert.strictEqual(endpoint[kState].pendingCallbacks, 0n);

    assert.deepStrictEqual(JSON.parse(JSON.stringify(endpoint[kState])), {
      isBound: false,
      isReceiving: false,
      isListening: false,
      isClosing: false,
      isBusy: false,
      pendingCallbacks: '0',
    });

    endpoint.busy = true;
    assert.strictEqual(endpoint[kState].isBusy, true);
    endpoint.busy = false;
    assert.strictEqual(endpoint[kState].isBusy, false);

    it('state can be inspected without errors', () => {
      assert.strictEqual(typeof inspect(endpoint[kState]), 'string');
    });
  });

  it('state is not readable after close', () => {
    const endpoint = new QuicEndpoint();
    endpoint[kState][kFinishClose]();
    assert.strictEqual(endpoint[kState].isBound, undefined);
  });

  it('state constructor argument is ArrayBuffer', () => {
    const endpoint = new QuicEndpoint();
    const Cons = endpoint[kState].constructor;
    assert.throws(() => new Cons(kPrivateConstructor, 1), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  it('endpoint stats', () => {
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

    it('stats can be inspected without errors', () => {
      assert.strictEqual(typeof inspect(endpoint.stats), 'string');
    });
  });

  it('stats are still readble after close', () => {
    const endpoint = new QuicEndpoint();
    assert.strictEqual(typeof endpoint.stats.toJSON(), 'object');
    endpoint.stats[kFinishClose]();
    assert.strictEqual(endpoint.stats.isConnected, false);
    assert.strictEqual(typeof endpoint.stats.destroyedAt, 'bigint');
    assert.strictEqual(typeof endpoint.stats.toJSON(), 'object');
  });

  it('stats constructor argument is ArrayBuffer', () => {
    const endpoint = new QuicEndpoint();
    const Cons = endpoint.stats.constructor;
    assert.throws(() => new Cons(kPrivateConstructor, 1), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });

  // TODO(@jasnell): The following tests are largely incomplete.
  // This is largely here to boost the code coverage numbers
  // temporarily while the rest of the functionality is being
  // implemented.
  it('stream and session states', () => {
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
  });

  it('stream and session stats', () => {
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
  });
});

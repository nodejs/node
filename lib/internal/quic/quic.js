'use strict';

// TODO(@jasnell) Temporarily ignoring c8 covrerage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  ArrayBufferPrototypeTransfer,
  ArrayIsArray,
  ArrayPrototypePush,
  ObjectDefineProperties,
  SafeSet,
  SymbolIterator,
  Uint8Array,
} = primordials;

// QUIC requires that Node.js be compiled with crypto support.
const {
  assertCrypto,
  SymbolAsyncDispose,
} = require('internal/util');
assertCrypto();

const { inspect } = require('internal/util/inspect');

const {
  Endpoint: Endpoint_,
  setCallbacks,

  // The constants to be exposed to end users for various options.
  CC_ALGO_RENO,
  CC_ALGO_CUBIC,
  CC_ALGO_BBR,
  CC_ALGO_RENO_STR,
  CC_ALGO_CUBIC_STR,
  CC_ALGO_BBR_STR,
  PREFERRED_ADDRESS_IGNORE,
  PREFERRED_ADDRESS_USE,
  DEFAULT_PREFERRED_ADDRESS_POLICY,
  DEFAULT_CIPHERS,
  DEFAULT_GROUPS,
  STREAM_DIRECTION_BIDIRECTIONAL,
  STREAM_DIRECTION_UNIDIRECTIONAL,

  // Internal constants for use by the implementation.
  // These are not exposed to end users.
  CLOSECONTEXT_CLOSE: kCloseContextClose,
  CLOSECONTEXT_BIND_FAILURE: kCloseContextBindFailure,
  CLOSECONTEXT_LISTEN_FAILURE: kCloseContextListenFailure,
  CLOSECONTEXT_RECEIVE_FAILURE: kCloseContextReceiveFailure,
  CLOSECONTEXT_SEND_FAILURE: kCloseContextSendFailure,
  CLOSECONTEXT_START_FAILURE: kCloseContextStartFailure,
} = internalBinding('quic');

const {
  isArrayBuffer,
  isArrayBufferView,
} = require('util/types');

const {
  Buffer,
} = require('buffer');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_STATE,
    ERR_QUIC_APPLICATION_ERROR,
    ERR_QUIC_CONNECTION_FAILED,
    ERR_QUIC_ENDPOINT_CLOSED,
    ERR_QUIC_OPEN_STREAM_FAILED,
    ERR_QUIC_TRANSPORT_ERROR,
    ERR_QUIC_VERSION_NEGOTIATION_ERROR,
  },
} = require('internal/errors');

const {
  InternalSocketAddress,
  SocketAddress,
  kHandle: kSocketAddressHandle,
} = require('internal/socketaddress');

const {
  isKeyObject,
  isCryptoKey,
} = require('internal/crypto/keys');

const {
  validateFunction,
  validateObject,
  validateString,
  validateBoolean,
} = require('internal/validators');

const kEmptyObject = { __proto__: null };

const {
  kBlocked,
  kDatagram,
  kDatagramStatus,
  kError,
  kFinishClose,
  kHandshake,
  kHeaders,
  kOwner,
  kRemoveSession,
  kNewSession,
  kRemoveStream,
  kNewStream,
  kPathValidation,
  kReset,
  kSessionTicket,
  kTrailers,
  kVersionNegotiation,
  kInspect,
  kKeyObjectHandle,
  kKeyObjectInner,
  kPrivateConstructor,
} = require('internal/quic/symbols');

const {
  QuicEndpointStats,
  QuicStreamStats,
  QuicSessionStats,
} = require('internal/quic/stats');

const {
  QuicEndpointState,
  QuicSessionState,
  QuicStreamState,
} = require('internal/quic/state');

const { assert } = require('internal/assert');

const dc = require('diagnostics_channel');
const onEndpointCreatedChannel = dc.channel('quic.endpoint.created');
const onEndpointListeningChannel = dc.channel('quic.endpoint.listen');
const onEndpointClosingChannel = dc.channel('quic.endpoint.closing');
const onEndpointClosedChannel = dc.channel('quic.endpoint.closed');
const onEndpointErrorChannel = dc.channel('quic.endpoint.error');
const onEndpointBusyChangeChannel = dc.channel('quic.endpoint.busy.change');
const onEndpointClientSessionChannel = dc.channel('quic.session.created.client');
const onEndpointServerSessionChannel = dc.channel('quic.session.created.server');
const onSessionOpenStreamChannel = dc.channel('quic.session.open.stream');
const onSessionReceivedStreamChannel = dc.channel('quic.session.received.stream');
const onSessionSendDatagramChannel = dc.channel('quic.session.send.datagram');
const onSessionUpdateKeyChannel = dc.channel('quic.session.update.key');
const onSessionClosingChannel = dc.channel('quic.session.closing');
const onSessionClosedChannel = dc.channel('quic.session.closed');
const onSessionReceiveDatagramChannel = dc.channel('quic.session.receive.datagram');
const onSessionReceiveDatagramStatusChannel = dc.channel('quic.session.receive.datagram.status');
const onSessionPathValidationChannel = dc.channel('quic.session.path.validation');
const onSessionTicketChannel = dc.channel('quic.session.ticket');
const onSessionVersionNegotiationChannel = dc.channel('quic.session.version.negotiation');
const onSessionHandshakeChannel = dc.channel('quic.session.handshake');

/**
 * @typedef {import('../socketaddress.js').SocketAddress} SocketAddress
 * @typedef {import('../crypto/keys.js').KeyObject} KeyObject
 * @typedef {import('../crypto/keys.js').CryptoKey} CryptoKey
 */

/**
 * @typedef {object} EndpointOptions
 * @property {SocketAddress} [address] The local address to bind to
 * @property {bigint|number} [retryTokenExpiration] The retry token expiration
 * @property {bigint|number} [tokenExpiration] The token expiration
 * @property {bigint|number} [maxConnectionsPerHost] The maximum number of connections per host
 * @property {bigint|number} [maxConnectionsTotal] The maximum number of total connections
 * @property {bigint|number} [maxStatelessResetsPerHost] The maximum number of stateless resets per host
 * @property {bigint|number} [addressLRUSize] The size of the address LRU cache
 * @property {bigint|number} [maxRetries] The maximum number of retries
 * @property {bigint|number} [maxPayloadSize] The maximum payload size
 * @property {bigint|number} [unacknowledgedPacketThreshold] The unacknowledged packet threshold
 * @property {bigint|number} [handshakeTimeout] The handshake timeout
 * @property {bigint|number} [maxStreamWindow] The maximum stream window
 * @property {bigint|number} [maxWindow] The maximum window
 * @property {number} [rxDiagnosticLoss] The receive diagnostic loss probability (range 0.0-1.0)
 * @property {number} [txDiagnosticLoss] The transmit diagnostic loss probability (range 0.0-1.0)
 * @property {number} [udpReceiveBufferSize] The UDP receive buffer size
 * @property {number} [udpSendBufferSize] The UDP send buffer size
 * @property {number} [udpTTL] The UDP TTL
 * @property {boolean} [noUdpPayloadSizeShaping] Disable UDP payload size shaping
 * @property {boolean} [validateAddress] Validate the address
 * @property {boolean} [disableActiveMigration] Disable active migration
 * @property {boolean} [ipv6Only] Use IPv6 only
 * @property {'reno'|'cubic'|'bbr'|number} [cc] The congestion control algorithm
 * @property {ArrayBufferView} [resetTokenSecret] The reset token secret
 * @property {ArrayBufferView} [tokenSecret] The token secret
 */

/**
 * @typedef {object} TlsOptions
 * @property {string} [sni] The server name indication
 * @property {string} [alpn] The application layer protocol negotiation
 * @property {string} [ciphers] The ciphers
 * @property {string} [groups] The groups
 * @property {boolean} [keylog] Enable key logging
 * @property {boolean} [verifyClient] Verify the client
 * @property {boolean} [tlsTrace] Enable TLS tracing
 * @property {boolean} [verifyPrivateKey] Verify the private key
 * @property {KeyObject|CryptoKey|Array<KeyObject|CryptoKey>} [keys] The keys
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [certs] The certificates
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [ca] The certificate authority
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [crl] The certificate revocation list
 */

/**
 * @typedef {object} TransportParams
 * @property {SocketAddress} [preferredAddressIpv4] The preferred IPv4 address
 * @property {SocketAddress} [preferredAddressIpv6] The preferred IPv6 address
 * @property {bigint|number} [initialMaxStreamDataBidiLocal] The initial maximum stream data bidirectional local
 * @property {bigint|number} [initialMaxStreamDataBidiRemote] The initial maximum stream data bidirectional remote
 * @property {bigint|number} [initialMaxStreamDataUni] The initial maximum stream data unidirectional
 * @property {bigint|number} [initialMaxData] The initial maximum data
 * @property {bigint|number} [initialMaxStreamsBidi] The initial maximum streams bidirectional
 * @property {bigint|number} [initialMaxStreamsUni] The initial maximum streams unidirectional
 * @property {bigint|number} [maxIdleTimeout] The maximum idle timeout
 * @property {bigint|number} [activeConnectionIDLimit] The active connection ID limit
 * @property {bigint|number} [ackDelayExponent] The acknowledgment delay exponent
 * @property {bigint|number} [maxAckDelay] The maximum acknowledgment delay
 * @property {bigint|number} [maxDatagramFrameSize] The maximum datagram frame size
 * @property {boolean} [disableActiveMigration] Disable active migration
 */

/**
 * @typedef {object} ApplicationOptions
 * @property {bigint|number} [maxHeaderPairs] The maximum header pairs
 * @property {bigint|number} [maxHeaderLength] The maximum header length
 * @property {bigint|number} [maxFieldSectionSize] The maximum field section size
 * @property {bigint|number} [qpackMaxDTableCapacity] The qpack maximum dynamic table capacity
 * @property {bigint|number} [qpackEncoderMaxDTableCapacity] The qpack encoder maximum dynamic table capacity
 * @property {bigint|number} [qpackBlockedStreams] The qpack blocked streams
 * @property {boolean} [enableConnectProtocol] Enable the connect protocol
 * @property {boolean} [enableDatagrams] Enable datagrams
 */

/**
 * @typedef {object} SessionOptions
 * @property {number} [version] The version
 * @property {number} [minVersion] The minimum version
 * @property {'use'|'ignore'|'default'} [preferredAddressPolicy] The preferred address policy
 * @property {ApplicationOptions} [application] The application options
 * @property {TransportParams} [transportParams] The transport parameters
 * @property {TlsOptions} [tls] The TLS options
 * @property {boolean} [qlog] Enable qlog
 * @property {ArrayBufferView} [sessionTicket] The session ticket
 */

/**
 * @typedef {object} Datagrams
 * @property {ReadableStream} readable The readable stream
 * @property {WritableStream} writable The writable stream
 */

/**
 * @typedef {object} Path
 * @property {SocketAddress} local The local address
 * @property {SocketAddress} remote The remote address
 */

/**
 * Called when the Endpoint receives a new server-side Session.
 * @callback OnSessionCallback
 * @this {QuicEndpoint}
 * @param {QuicSession} session
 * @returns {void}
 */

/**
 * @callback OnStreamCallback
 * @this {QuicSession}
 * @param {QuicStream} stream
 * @returns {void}
 */

/**
 * @callback OnDatagramCallback
 * @this {QuicSession}
 * @param {Uint8Array} datagram
 * @param {boolean} early A datagram is early if it was received before the TLS handshake completed
 * @returns {void}
 */

/**
 * @callback OnDatagramStatusCallback
 * @this {QuicSession}
 * @param {bigint} id
 * @param {'lost'|'acknowledged'} status
 * @returns {void}
 */

/**
 * @callback OnPathValidationCallback
 * @this {QuicSession}
 * @param {'aborted'|'failure'|'success'} result
 * @param {SocketAddress} newLocalAddress
 * @param {SocketAddress} newRemoteAddress
 * @param {SocketAddress} oldLocalAddress
 * @param {SocketAddress} oldRemoteAddress
 * @param {boolean} preferredAddress
 * @returns {void}
 */

/**
 * @callback OnSessionTicketCallback
 * @this {QuicSession}
 * @param {object} ticket
 * @returns {void}
 */

/**
 * @callback OnVersionNegotiationCallback
 * @this {QuicSession}
 * @param {number} version
 * @param {number[]} requestedVersions
 * @param {number[]} supportedVersions
 * @returns {void}
 */

/**
 * @callback OnHandshakeCallback
 * @this {QuicSession}
 * @param {string} sni
 * @param {string} alpn
 * @param {string} cipher
 * @param {string} cipherVersion
 * @param {string} validationErrorReason
 * @param {number} validationErrorCode
 * @param {boolean} earlyDataAccepted
 * @returns {void}
 */

/**
 * @callback OnBlockedCallback
 * @param {QuicStream} stream
 * @returns {void}
 */

/**
 * @callback OnStreamErrorCallback
 * @param {any} error
 * @param {QuicStream} stream
 * @returns {void}
 */

/**
 * @callback OnHeadersCallback
 * @param {object} headers
 * @param {string} kind
 * @param {QuicStream} stream
 * @returns {void}
 */

/**
 * @callback OnTrailersCallback
 * @param {QuicStream} stream
 * @returns {void}
 */

/**
 * @typedef {object} StreamCallbackConfiguration
 * @property {OnBlockedCallback} [onblocked] The blocked callback
 * @property {OnStreamErrorCallback} [onreset] The reset callback
 * @property {OnHeadersCallback} [onheaders] The headers callback
 * @property {OnTrailersCallback} [ontrailers] The trailers callback
 */

/**
 * Provdes the callback configuration for Sessions.
 * @typedef {object} SessionCallbackConfiguration
 * @property {OnStreamCallback} onstream The stream callback
 * @property {OnDatagramCallback} [ondatagram] The datagram callback
 * @property {OnDatagramStatusCallback} [ondatagramstatus] The datagram status callback
 * @property {OnPathValidationCallback} [onpathvalidation] The path validation callback
 * @property {OnSessionTicketCallback} [onsessionticket] The session ticket callback
 * @property {OnVersionNegotiationCallback} [onversionnegotiation] The version negotiation callback
 * @property {OnHandshakeCallback} [onhandshake] The handshake callback
 */

/**
 * @typedef {object} ProcessedSessionCallbackConfiguration
 * @property {OnStreamCallback} onstream The stream callback
 * @property {OnDatagramCallback} [ondatagram] The datagram callback
 * @property {OnDatagramStatusCallback} [ondatagramstatus] The datagram status callback
 * @property {OnPathValidationCallback} [onpathvalidation] The path validation callback
 * @property {OnSessionTicketCallback} [onsessionticket] The session ticket callback
 * @property {OnVersionNegotiationCallback} [onversionnegotiation] The version negotation callback
 * @property {OnHandshakeCallback} [onhandshake] The handshake callback
 * @property {StreamCallbackConfiguration} stream The processed stream callbacks
 */

/**
 * Provides the callback configuration for the Endpoint.
 * @typedef {object} EndpointCallbackConfiguration
 * @property {OnSessionCallback} onsession The session callback
 * @property {OnStreamCallback} onstream The stream callback
 * @property {OnDatagramCallback} [ondatagram] The datagram callback
 * @property {OnDatagramStatusCallback} [ondatagramstatus] The datagram status callback
 * @property {OnPathValidationCallback} [onpathvalidation] The path validation callback
 * @property {OnSessionTicketCallback} [onsessionticket] The session ticket callback
 * @property {OnVersionNegotiationCallback} [onversionnegotiation] The version negotiation callback
 * @property {OnHandshakeCallback} [onhandshake] The handshake callback
 * @property {OnBlockedCallback} [onblocked] The blocked callback
 * @property {OnStreamErrorCallback} [onreset] The reset callback
 * @property {OnHeadersCallback} [onheaders] The headers callback
 * @property {OnTrailersCallback} [ontrailers] The trailers callback
 * @property {SocketAddress} [address] The local address to bind to
 * @property {bigint|number} [retryTokenExpiration] The retry token expiration
 * @property {bigint|number} [tokenExpiration] The token expiration
 * @property {bigint|number} [maxConnectionsPerHost] The maximum number of connections per host
 * @property {bigint|number} [maxConnectionsTotal] The maximum number of total connections
 * @property {bigint|number} [maxStatelessResetsPerHost] The maximum number of stateless resets per host
 * @property {bigint|number} [addressLRUSize] The size of the address LRU cache
 * @property {bigint|number} [maxRetries] The maximum number of retries
 * @property {bigint|number} [maxPayloadSize] The maximum payload size
 * @property {bigint|number} [unacknowledgedPacketThreshold] The unacknowledged packet threshold
 * @property {bigint|number} [handshakeTimeout] The handshake timeout
 * @property {bigint|number} [maxStreamWindow] The maximum stream window
 * @property {bigint|number} [maxWindow] The maximum window
 * @property {number} [rxDiagnosticLoss] The receive diagnostic loss probability (range 0.0-1.0)
 * @property {number} [txDiagnosticLoss] The transmit diagnostic loss probability (range 0.0-1.0)
 * @property {number} [udpReceiveBufferSize] The UDP receive buffer size
 * @property {number} [udpSendBufferSize] The UDP send buffer size
 * @property {number} [udpTTL] The UDP TTL
 * @property {boolean} [noUdpPayloadSizeShaping] Disable UDP payload size shaping
 * @property {boolean} [validateAddress] Validate the address
 * @property {boolean} [disableActiveMigration] Disable active migration
 * @property {boolean} [ipv6Only] Use IPv6 only
 * @property {'reno'|'cubic'|'bbr'|number} [cc] The congestion control algorithm
 * @property {ArrayBufferView} [resetTokenSecret] The reset token secret
 * @property {ArrayBufferView} [tokenSecret] The token secret
 */

/**
 * @typedef {object} ProcessedEndpointCallbackConfiguration
 * @property {OnSessionCallback} onsession The session callback
 * @property {SessionCallbackConfiguration} session The processesd session callbacks
 */

setCallbacks({
  // QuicEndpoint callbacks

  /**
   * Called when the QuicEndpoint C++ handle has closed and we need to finish
   * cleaning up the JS side.
   * @param {number} context Identifies the reason the endpoint was closed.
   * @param {number} status If context indicates an error, provides the error code.
   */
  onEndpointClose(context, status) {
    this[kOwner][kFinishClose](context, status);
  },
  /**
   * Called when the QuicEndpoint C++ handle receives a new server-side session
   * @param {*} session The QuicSession C++ handle
   */
  onSessionNew(session) {
    this[kOwner][kNewSession](session);
  },

  // QuicSession callbacks

  /**
   * Called when the underlying session C++ handle is closed either normally
   * or with an error.
   * @param {number} errorType
   * @param {number} code
   * @param {string} [reason]
   */
  onSessionClose(errorType, code, reason) {
    this[kOwner][kFinishClose](errorType, code, reason);
  },

  /**
   * Called when a datagram is received on this session.
   * @param {Uint8Array} uint8Array
   * @param {boolean} early
   */
  onSessionDatagram(uint8Array, early) {
    this[kOwner][kDatagram](uint8Array, early);
  },

  /**
   * Called when the status of a datagram is received.
   * @param {bigint} id
   * @param {'lost' | 'acknowledged'} status
   */
  onSessionDatagramStatus(id, status) {
    this[kOwner][kDatagramStatus](id, status);
  },

  /**
   * Called when the session handshake completes.
   * @param {string} sni
   * @param {string} alpn
   * @param {string} cipher
   * @param {string} cipherVersion
   * @param {string} validationErrorReason
   * @param {number} validationErrorCode
   * @param {boolean} earlyDataAccepted
   */
  onSessionHandshake(sni, alpn, cipher, cipherVersion,
                     validationErrorReason,
                     validationErrorCode,
                     earlyDataAccepted) {
    this[kOwner][kHandshake](sni, alpn, cipher, cipherVersion,
                             validationErrorReason, validationErrorCode,
                             earlyDataAccepted);
  },

  /**
   * Called when the session path validation completes.
   * @param {'aborted'|'failure'|'success'} result
   * @param {SocketAddress} newLocalAddress
   * @param {SocketAddress} newRemoteAddress
   * @param {SocketAddress} oldLocalAddress
   * @param {SocketAddress} oldRemoteAddress
   * @param {boolean} preferredAddress
   */
  onSessionPathValidation(result, newLocalAddress, newRemoteAddress,
                          oldLocalAddress, oldRemoteAddress, preferredAddress) {
    this[kOwner][kPathValidation](result, newLocalAddress, newRemoteAddress,
                                  oldLocalAddress, oldRemoteAddress,
                                  preferredAddress);
  },

  /**
   * Called when the session generates a new TLS session ticket
   * @param {object} ticket An opaque session ticket
   */
  onSessionTicket(ticket) {
    this[kOwner][kSessionTicket](ticket);
  },

  /**
   * Called when the session receives a session version negotiation request
   * @param {*} version
   * @param {*} requestedVersions
   * @param {*} supportedVersions
   */
  onSessionVersionNegotiation(version,
                              requestedVersions,
                              supportedVersions) {
    this[kOwner][kVersionNegotiation](version, requestedVersions, supportedVersions);
    // Note that immediately following a version negotiation event, the
    // session will be destroyed.
  },

  /**
   * Called when a new stream has been received for the session
   * @param {object} stream The QuicStream C++ handle
   * @param {number} direction The stream direction (0 == bidi, 1 == uni)
   */
  onStreamCreated(stream, direction) {
    const session = this[kOwner];
    // The event is ignored and the stream destroyed if the session has been destroyed.
    if (session.destroyed) {
      stream.destroy();
      return;
    };
    session[kNewStream](stream, direction);
  },

  // QuicStream callbacks
  onStreamBlocked() {
    // Called when the stream C++ handle has been blocked by flow control.
    this[kOwner][kBlocked]();
  },
  onStreamClose(error) {
    // Called when the stream C++ handle has been closed.
    this[kOwner][kError](error);
  },
  onStreamReset(error) {
    // Called when the stream C++ handle has received a stream reset.
    this[kOwner][kReset](error);
  },
  onStreamHeaders(headers, kind) {
    // Called when the stream C++ handle has received a full block of headers.
    this[kOwner][kHeaders](headers, kind);
  },
  onStreamTrailers() {
    // Called when the stream C++ handle is ready to receive trailing headers.
    this[kOwner][kTrailers]();
  },
});

class QuicStream {
  /** @type {object} */
  #handle;
  /** @type {QuicSession} */
  #session;
  /** @type {QuicStreamStats} */
  #stats;
  /** @type {QuicStreamState} */
  #state;
  /** @type {number} */
  #direction;
  /** @type {OnBlockedCallback|undefined} */
  #onblocked;
  /** @type {OnStreamErrorCallback|undefined} */
  #onreset;
  /** @type {OnHeadersCallback|undefined} */
  #onheaders;
  /** @type {OnTrailersCallback|undefined} */
  #ontrailers;

  /**
   * @param {symbol} privateSymbol
   * @param {StreamCallbackConfiguration} config
   * @param {object} handle
   * @param {QuicSession} session
   */
  constructor(privateSymbol, config, handle, session, direction) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }

    const {
      onblocked,
      onreset,
      onheaders,
      ontrailers,
    } = config;

    if (onblocked !== undefined) {
      this.#onblocked = onblocked.bind(this);
    }
    if (onreset !== undefined) {
      this.#onreset = onreset.bind(this);
    }
    if (onheaders !== undefined) {
      this.#onheaders = onheaders.bind(this);
    }
    if (ontrailers !== undefined) {
      this.#ontrailers = ontrailers.bind(this);
    }
    this.#handle = handle;
    this.#handle[kOwner] = true;

    this.#session = session;
    this.#direction = direction;

    this.#stats = new QuicStreamStats(kPrivateConstructor, this.#handle.stats);

    this.#state = new QuicStreamState(kPrivateConstructor, this.#handle.stats);
    this.#state.wantsBlock = !!this.#onblocked;
    this.#state.wantsReset = !!this.#onreset;
    this.#state.wantsHeaders = !!this.#onheaders;
    this.#state.wantsTrailers = !!this.#ontrailers;
  }

  /** @type {QuicStreamStats} */
  get stats() { return this.#stats; }

  /** @type {QuicStreamState} */
  get state() { return this.#state; }

  /** @type {QuicSession} */
  get session() { return this.#session; }

  /** @type {bigint} */
  get id() { return this.#state.id; }

  /** @type {'bidi'|'uni'} */
  get direction() {
    return this.#direction === 0 ? 'bidi' : 'uni';
  }

  /** @returns {boolean} */
  get destroyed() {
    return this.#handle === undefined;
  }

  destroy(error) {
    if (this.destroyed) return;
    // TODO(@jasnell): pass an error code
    this.#stats[kFinishClose]();
    this.#state[kFinishClose]();
    this.#onblocked = undefined;
    this.#onreset = undefined;
    this.#onheaders = undefined;
    this.#ontrailers = undefined;
    this.#session[kRemoveStream](this);
    this.#session = undefined;
    this.#handle.destroy();
    this.#handle = undefined;
  }

  [kBlocked]() {
    // The blocked event should only be called if the stream was created with
    // an onblocked callback. The callback should always exist here.
    assert(this.#onblocked, 'Unexpected stream blocked event');
    this.#onblocked(this);
  }

  [kError](error) {
    this.destroy(error);
  }

  [kReset](error) {
    // The reset event should only be called if the stream was created with
    // an onreset callback. The callback should always exist here.
    assert(this.#onreset, 'Unexpected stream reset event');
    this.#onreset(error, this);
  }

  [kHeaders](headers, kind) {
    // The headers event should only be called if the stream was created with
    // an onheaders callback. The callback should always exist here.
    assert(this.#onheaders, 'Unexpected stream headers event');
    this.#onheaders(headers, kind, this);
  }

  [kTrailers]() {
    // The trailers event should only be called if the stream was created with
    // an ontrailers callback. The callback should always exist here.
    assert(this.#ontrailers, 'Unexpected stream trailers event');
    this.#ontrailers(this);
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `Stream ${inspect({
      id: this.id,
      direction: this.direction,
      stats: this.stats,
      state: this.state,
      session: this.session,
    }, opts)}`;
  }
}

class QuicSession {
  /** @type {QuicEndpoint} */
  #endpoint = undefined;
  /** @type {boolean} */
  #isPendingClose = false;
  /** @type {object|undefined} */
  #handle;
  /** @type {PromiseWithResolvers<void>} */
  #pendingClose = Promise.withResolvers();  // eslint-disable-line node-core/prefer-primordials
  /** @type {SocketAddress|undefined} */
  #remoteAddress = undefined;
  /** @type {QuicSessionState} */
  #state;
  /** @type {QuicSessionStats} */
  #stats;
  /** @type {Set<QuicStream>} */
  #streams = new SafeSet();
  /** @type {OnStreamCallback} */
  #onstream;
  /** @type {OnDatagramCallback|undefined} */
  #ondatagram;
  /** @type {OnDatagramStatusCallback|undefined} */
  #ondatagramstatus;
  /** @type {OnPathValidationCallback|undefined} */
  #onpathvalidation;
  /** @type {OnSessionTicketCallback|undefined} */
  #onsessionticket;
  /** @type {OnVersionNegotiationCallback|undefined} */
  #onversionnegotiation;
  /** @type {OnHandshakeCallback} */
  #onhandshake;
  /** @type {StreamCallbackConfiguration} */
  #streamConfig;

  /**
   * @param {symbol} privateSymbol
   * @param {ProcessedSessionCallbackConfiguration} config
   * @param {object} handle
   * @param {QuicEndpoint} endpoint
   */
  constructor(privateSymbol, config, handle, endpoint) {
    // Instances of QuicSession can only be created internally.
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    // The config should have already been validated by the QuicEndpoing
    const {
      ondatagram,
      ondatagramstatus,
      onhandshake,
      onpathvalidation,
      onsessionticket,
      onstream,
      onversionnegotiation,
      stream,
    } = config;

    if (ondatagram !== undefined) {
      this.#ondatagram = ondatagram.bind(this);
    }
    if (ondatagramstatus !== undefined) {
      this.#ondatagramstatus = ondatagramstatus.bind(this);
    }
    if (onpathvalidation !== undefined) {
      this.#onpathvalidation = onpathvalidation.bind(this);
    }
    if (onsessionticket !== undefined) {
      this.#onsessionticket = onsessionticket.bind(this);
    }
    if (onversionnegotiation !== undefined) {
      this.#onversionnegotiation = onversionnegotiation.bind(this);
    }
    if (onhandshake !== undefined) {
      this.#onhandshake = onhandshake.bind(this);
    }

    // It is ok for onstream to be undefined if the session is not expecting
    // or wanting to receive incoming streams. If a stream is received and
    // no onstream callback is specified, a warning will be emitted and the
    // stream will just be immediately destroyed.
    if (onstream !== undefined) {
      this.#onstream = onstream.bind(this);
    }
    this.#endpoint = endpoint;
    this.#streamConfig = stream;

    this.#handle = handle;
    this.#handle[kOwner] = this;
    this.#stats = new QuicSessionStats(kPrivateConstructor, handle.stats);

    this.#state = new QuicSessionState(kPrivateConstructor, handle.state);
    this.#state.hasDatagramListener = !!ondatagram;
    this.#state.hasPathValidationListener = !!onpathvalidation;
    this.#state.hasSessionTicketListener = !!onsessionticket;
    this.#state.hasVersionNegotiationListener = !!onversionnegotiation;
  }

  /** @type {boolean} */
  get #isClosedOrClosing() {
    return this.#handle === undefined || this.#isPendingClose;
  }

  /** @type {QuicSessionStats} */
  get stats() { return this.#stats; }

  /** @type {QuicSessionState} */
  get state() { return this.#state; }

  /** @type {QuicEndpoint} */
  get endpoint() { return this.#endpoint; }

  /**
   * The path is the local and remote addresses of the session.
   * @type {Path}
   */
  get path() {
    if (this.destroyed) return undefined;
    if (this.#remoteAddress === undefined) {
      const addr = this.#handle.getRemoteAddress();
      if (addr !== undefined) {
        this.#remoteAddress = new InternalSocketAddress(addr);
      }
    }
    return {
      local: this.#endpoint.address,
      remote: this.#remoteAddress,
    };
  }

  /**
   * @returns {QuicStream}
   */
  openBidirectionalStream() {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }
    if (!this.state.isStreamOpenAllowed) {
      throw new ERR_QUIC_OPEN_STREAM_FAILED();
    }
    const handle = this.#handle.openStream(STREAM_DIRECTION_BIDIRECTIONAL);
    if (handle === undefined) {
      throw new ERR_QUIC_OPEN_STREAM_FAILED();
    }
    const stream = new QuicStream(kPrivateConstructor, this.#streamConfig, handle,
                                  this, 0 /* Bidirectional */);
    this.#streams.add(stream);

    if (onSessionOpenStreamChannel.hasSubscribers) {
      onSessionOpenStreamChannel.publish({
        stream,
        session: this,
      });
    }
    return stream;
  }

  /**
   * @returns {QuicStream}
   */
  openUnidirectionalStream() {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }
    if (!this.state.isStreamOpenAllowed) {
      throw new ERR_QUIC_OPEN_STREAM_FAILED();
    }
    const handle = this.#handle.openStream(STREAM_DIRECTION_UNIDIRECTIONAL);
    if (handle === undefined) {
      throw new ERR_QUIC_OPEN_STREAM_FAILED();
    }
    const stream = new QuicStream(kPrivateConstructor, this.#streamConfig, handle,
                                  this, 1 /* Unidirectional */);
    this.#streams.add(stream);

    if (onSessionOpenStreamChannel.hasSubscribers) {
      onSessionOpenStreamChannel.publish({
        stream,
        session: this,
      });
    }

    return stream;
  }

  /**
   * Send a datagram. The id of the sent datagram will be returned. The status
   * of the sent datagram will be reported via the datagram-status event if
   * possible.
   *
   * If a string is given it will be encoded as UTF-8.
   *
   * If an ArrayBufferView is given, the view will be copied.
   * @param {ArrayBufferView|string} datagram The datagram payload
   * @returns {bigint} The datagram ID
   */
  sendDatagram(datagram) {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }
    if (typeof datagram === 'string') {
      datagram = Buffer.from(datagram, 'utf8');
    } else {
      if (!isArrayBufferView(datagram)) {
        throw new ERR_INVALID_ARG_TYPE('datagram',
                                       ['ArrayBufferView', 'string'],
                                       datagram);
      }
      datagram = new Uint8Array(ArrayBufferPrototypeTransfer(datagram.buffer),
                                datagram.byteOffset,
                                datagram.byteLength);
    }
    const id = this.#handle.sendDatagram(datagram);

    if (onSessionSendDatagramChannel.hasSubscribers) {
      onSessionSendDatagramChannel.publish({
        id,
        length: datagram.byteLength,
        session: this,
      });
    }

    return id;
  }

  /**
   * Initiate a key update.
   */
  updateKey() {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }
    this.#handle.updateKey();
    if (onSessionUpdateKeyChannel.hasSubscribers) {
      onSessionUpdateKeyChannel.publish({
        session: this,
      });
    }
  }

  /**
   * Gracefully closes the session. Any streams created on the session will be
   * allowed to complete gracefully and any datagrams that have already been
   * queued for sending will be allowed to complete. Once all streams have been
   * completed and all datagrams have been sent, the session will be closed.
   * New streams will not be allowed to be created. The returned promise will
   * be resolved when the session closes, or will be rejected if the session
   * closes abruptly due to an error.
   * @returns {Promise<void>}
   */
  close() {
    if (!this.#isClosedOrClosing) {
      this.#isPendingClose = true;
      this.#handle?.gracefulClose();
      if (onSessionClosingChannel.hasSubscribers) {
        onSessionClosingChannel.publish({
          session: this,
        });
      }
    }
    return this.closed;
  }

  /**
   * A promise that is resolved when the session is closed, or is rejected if
   * the session is closed abruptly due to an error.
   * @type {Promise<void>}
   */
  get closed() { return this.#pendingClose.promise; }

  /** @type {boolean} */
  get destroyed() { return this.#handle === undefined; }

  /**
   * Forcefully closes the session abruptly without waiting for streams to be
   * completed naturally. Any streams that are still open will be immediately
   * destroyed and any queued datagrams will be dropped. If an error is given,
   * the closed promise will be rejected with that error. If no error is given,
   * the closed promise will be resolved.
   * @param {any} error
   * @return {Promise<void>} Returns this.closed
   */
  destroy(error) {
    if (this.destroyed) return;
    // First, forcefully and immediately destroy all open streams, if any.
    for (const stream of this.#streams) {
      stream.destroy(error);
    }
    // The streams should remove themselves when they are destroyed but let's
    // be doubly sure.
    if (this.#streams.size) {
      process.emitWarning(
        `The session is destroyed with ${this.#streams.size} active streams. ` +
        'This should not happen and indicates a bug in Node.js. Please open an ' +
        'issue in the Node.js GitHub repository at https://github.com/nodejs/node ' +
        'to report the problem.',
      );
    }
    this.#streams.clear();

    // Remove this session immediately from the endpoint
    this.#endpoint[kRemoveSession](this);
    this.#endpoint = undefined;
    this.#isPendingClose = false;

    if (error) {
      // If the session is still waiting to be closed, and error
      // is specified, reject the closed promise.
      this.#pendingClose.reject?.(error);
    } else {
      this.#pendingClose.resolve?.();
    }
    this.#pendingClose.reject = undefined;
    this.#pendingClose.resolve = undefined;

    this.#remoteAddress = undefined;
    this.#state[kFinishClose]();
    this.#stats[kFinishClose]();

    this.#onstream = undefined;
    this.#ondatagram = undefined;
    this.#ondatagramstatus = undefined;
    this.#onpathvalidation = undefined;
    this.#onsessionticket = undefined;
    this.#onversionnegotiation = undefined;
    this.#onhandshake = undefined;
    this.#streamConfig = undefined;

    // Destroy the underlying C++ handle
    this.#handle.destroy();
    this.#handle = undefined;

    if (onSessionClosedChannel.hasSubscribers) {
      onSessionClosedChannel.publish({
        session: this,
      });
    }

    return this.closed;
  }

  /**
   * @param {number} errorType
   * @param {number} code
   * @param {string} [reason]
   */
  [kFinishClose](errorType, code, reason) {
    // If code is zero, then we closed without an error. Yay! We can destroy
    // safely without specifying an error.
    if (code === 0) {
      this.destroy();
      return;
    }

    // Otherwise, errorType indicates the type of error that occurred, code indicates
    // the specific error, and reason is an optional string describing the error.
    switch (errorType) {
      case 0: /* Transport Error */
        this.destroy(new ERR_QUIC_TRANSPORT_ERROR(code, reason));
        break;
      case 1: /* Application Error */
        this.destroy(new ERR_QUIC_APPLICATION_ERROR(code, reason));
        break;
      case 2: /* Version Negotiation Error */
        this.destroy(new ERR_QUIC_VERSION_NEGOTIATION_ERROR());
        break;
      case 3: /* Idle close */ {
        // An idle close is not really an error. We can just destroy.
        this.destroy();
        break;
      }
    }
  }

  /**
   * @param {Uint8Array} u8
   * @param {boolean} early
   */
  [kDatagram](u8, early) {
    // The datagram event should only be called if the session was created with
    // an ondatagram callback. The callback should always exist here.
    assert(this.#ondatagram, 'Unexpected datagram event');
    if (this.destroyed) return;
    this.#ondatagram(u8, early);

    if (onSessionReceiveDatagramChannel.hasSubscribers) {
      onSessionReceiveDatagramChannel.publish({
        length: u8.byteLength,
        early,
        session: this,
      });
    }
  }

  /**
   * @param {bigint} id
   * @param {'lost'|'acknowledged'} status
   */
  [kDatagramStatus](id, status) {
    if (this.destroyed) return;
    // The ondatagramstatus callback may not have been specified. That's ok.
    // We'll just ignore the event in that case.
    this.#ondatagramstatus?.(id, status);

    if (onSessionReceiveDatagramStatusChannel.hasSubscribers) {
      onSessionReceiveDatagramStatusChannel.publish({
        id,
        status,
        session: this,
      });
    }
  }

  /**
   * @param {'aborted'|'failure'|'success'} result
   * @param {SocketAddress} newLocalAddress
   * @param {SocketAddress} newRemoteAddress
   * @param {SocketAddress} oldLocalAddress
   * @param {SocketAddress} oldRemoteAddress
   * @param {boolean} preferredAddress
   */
  [kPathValidation](result, newLocalAddress, newRemoteAddress, oldLocalAddress,
                    oldRemoteAddress, preferredAddress) {
    // The path validation event should only be called if the session was created
    // with an onpathvalidation callback. The callback should always exist here.
    assert(this.#onpathvalidation, 'Unexpected path validation event');
    if (this.destroyed) return;
    this.#onpathvalidation(result, newLocalAddress, newRemoteAddress,
                           oldLocalAddress, oldRemoteAddress, preferredAddress);

    if (onSessionPathValidationChannel.hasSubscribers) {
      onSessionPathValidationChannel.publish({
        result,
        newLocalAddress,
        newRemoteAddress,
        oldLocalAddress,
        oldRemoteAddress,
        preferredAddress,
        session: this,
      });
    }
  }

  /**
   * @param {object} ticket
   */
  [kSessionTicket](ticket) {
    // The session ticket event should only be called if the session was created
    // with an onsessionticket callback. The callback should always exist here.
    assert(this.#onsessionticket, 'Unexpected session ticket event');
    if (this.destroyed) return;
    this.#onsessionticket(ticket);
    if (onSessionTicketChannel.hasSubscribers) {
      onSessionTicketChannel.publish({
        ticket,
        session: this,
      });
    }
  }

  /**
   * @param {number} version
   * @param {number[]} requestedVersions
   * @param {number[]} supportedVersions
   */
  [kVersionNegotiation](version, requestedVersions, supportedVersions) {
    // The version negotiation event should only be called if the session was
    // created with an onversionnegotiation callback. The callback should always
    // exist here.
    if (this.destroyed) return;
    this.#onversionnegotiation(version, requestedVersions, supportedVersions);
    this.destroy(new ERR_QUIC_VERSION_NEGOTIATION_ERROR());

    if (onSessionVersionNegotiationChannel.hasSubscribers) {
      onSessionVersionNegotiationChannel.publish({
        version,
        requestedVersions,
        supportedVersions,
        session: this,
      });
    }
  }

  /**
   * @param {string} sni
   * @param {string} alpn
   * @param {string} cipher
   * @param {string} cipherVersion
   * @param {string} validationErrorReason
   * @param {number} validationErrorCode
   * @param {boolean} earlyDataAccepted
   */
  [kHandshake](sni, alpn, cipher, cipherVersion, validationErrorReason,
               validationErrorCode, earlyDataAccepted) {
    if (this.destroyed) return;
    // The onhandshake callback may not have been specified. That's ok.
    // We'll just ignore the event in that case.
    this.#onhandshake?.(sni, alpn, cipher, cipherVersion, validationErrorReason,
                        validationErrorCode, earlyDataAccepted);

    if (onSessionHandshakeChannel.hasSubscribers) {
      onSessionHandshakeChannel.publish({
        sni,
        alpn,
        cipher,
        cipherVersion,
        validationErrorReason,
        validationErrorCode,
        earlyDataAccepted,
        session: this,
      });
    }
  }

  /**
   * @param {object} handle
   * @param {number} direction
   */
  [kNewStream](handle, direction) {
    const stream = new QuicStream(kPrivateConstructor, this.#streamConfig, handle,
                                  this, direction);

    // A new stream was received. If we don't have an onstream callback, then
    // there's nothing we can do about it. Destroy the stream in this case.
    if (this.#onstream === undefined) {
      process.emitWarning('A new stream was received but no onstream callback was provided');
      stream.destroy();
      return;
    }
    this.#streams.add(stream);

    this.#onstream(stream);

    if (onSessionReceivedStreamChannel.hasSubscribers) {
      onSessionReceivedStreamChannel.publish({
        stream,
        session: this,
      });
    }
  }

  [kRemoveStream](stream) {
    this.#streams.delete(stream);
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `QuicSession ${inspect({
      closed: this.closed,
      closing: this.#isPendingClose,
      destroyed: this.destroyed,
      endpoint: this.endpoint,
      path: this.path,
      state: this.state,
      stats: this.stats,
      streams: this.#streams,
    }, opts)}`;
  }

  async [SymbolAsyncDispose]() { await this.close(); }
}

class QuicEndpoint {
  /**
   * The local socket address on which the endpoint is listening (lazily created)
   * @type {SocketAddress|undefined}
   */
  #address = undefined;
  /**
   * When true, the endpoint has been marked busy and is temporarily not accepting
   * new sessions (only used when the Endpoint is acting as a server)
   * @type {boolean}
   */
  #busy = false;
  /**
   * The underlying C++ handle for the endpoint. When undefined the endpoint is
   * considered to be closed.
   * @type {object}
   */
  #handle;
  /**
   * True if endpoint.close() has been called and the [kFinishClose] method has
   * not yet been called.
   * @type {boolean}
   */
  #isPendingClose = false;
  /**
   * True if the endpoint is acting as a server and actively listening for connections.
   * @type {boolean}
   */
  #listening = false;
  /**
   * A promise that is resolved when the endpoint has been closed (or rejected if
   * the endpoint closes abruptly due to an error).
   * @type {PromiseWithResolvers<void>}
   */
  #pendingClose = Promise.withResolvers();  // eslint-disable-line node-core/prefer-primordials
  /**
   * If destroy() is called with an error, the error is stored here and used to reject
   * the pendingClose promise when [kFinishClose] is called.
   * @type {any}
   */
  #pendingError = undefined;
  /**
   * The collection of active sessions.
   * @type {Set<QuicSession>}
   */
  #sessions = new SafeSet();
  /**
   * The internal state of the endpoint. Used to efficiently track and update the
   * state of the underlying c++ endpoint handle.
   * @type {QuicEndpointState}
   */
  #state;
  /**
   * The collected statistics for the endpoint.
   * @type {QuicEndpointStats}
   */
  #stats;
  /**
   * The user provided callback that is invoked when a new session is received.
   * (used only when the endpoint is acting as a server)
   * @type {OnSessionCallback}
   */
  #onsession;
  /**
   * The callback configuration used for new sessions (client or server)
   * @type {ProcessedSessionCallbackConfiguration}
   */
  #sessionConfig;

  /**
   * @param {EndpointCallbackConfiguration} config
   * @returns {StreamCallbackConfiguration}
   */
  #processStreamConfig(config) {
    validateObject(config, 'config');
    const {
      onblocked,
      onreset,
      onheaders,
      ontrailers,
    } = config;

    if (onblocked !== undefined) {
      validateFunction(onblocked, 'config.onblocked');
    }
    if (onreset !== undefined) {
      validateFunction(onreset, 'config.onreset');
    }
    if (onheaders !== undefined) {
      validateFunction(onheaders, 'config.onheaders');
    }
    if (ontrailers !== undefined) {
      validateFunction(ontrailers, 'ontrailers');
    }

    return {
      __proto__: null,
      onblocked,
      onreset,
      onheaders,
      ontrailers,
    };
  }

  /**
   *
   * @param {EndpointCallbackConfiguration} config
   * @returns {ProcessedSessionCallbackConfiguration}
   */
  #processSessionConfig(config) {
    validateObject(config, 'config');
    const {
      onstream,
      ondatagram,
      ondatagramstatus,
      onpathvalidation,
      onsessionticket,
      onversionnegotiation,
      onhandshake,
    } = config;
    if (onstream !== undefined) {
      validateFunction(onstream, 'config.onstream');
    }
    if (ondatagram !== undefined) {
      validateFunction(ondatagram, 'config.ondatagram');
    }
    if (ondatagramstatus !== undefined) {
      validateFunction(ondatagramstatus, 'config.ondatagramstatus');
    }
    if (onpathvalidation !== undefined) {
      validateFunction(onpathvalidation, 'config.onpathvalidation');
    }
    if (onsessionticket !== undefined) {
      validateFunction(onsessionticket, 'config.onsessionticket');
    }
    if (onversionnegotiation !== undefined) {
      validateFunction(onversionnegotiation, 'config.onversionnegotiation');
    }
    if (onhandshake !== undefined) {
      validateFunction(onhandshake, 'config.onhandshake');
    }
    return {
      __proto__: null,
      onstream,
      ondatagram,
      ondatagramstatus,
      onpathvalidation,
      onsessionticket,
      onversionnegotiation,
      onhandshake,
      stream: this.#processStreamConfig(config),
    };
  }

  /**
   * @param {EndpointCallbackConfiguration} config
   * @returns {ProcessedEndpointCallbackConfiguration}
   */
  #processEndpointConfig(config) {
    validateObject(config, 'config');
    const {
      onsession,
    } = config;

    if (onsession !== undefined) {
      validateFunction(config.onsession, 'config.onsession');
    }

    return {
      __proto__: null,
      onsession,
      session: this.#processSessionConfig(config),
    };
  }

  /**
   * @param {EndpointCallbackConfiguration} options
   * @returns {EndpointOptions}
   */
  #processEndpointOptions(options) {
    validateObject(options, 'options');
    let { address } = options;
    const {
      retryTokenExpiration,
      tokenExpiration,
      maxConnectionsPerHost,
      maxConnectionsTotal,
      maxStatelessResetsPerHost,
      addressLRUSize,
      maxRetries,
      maxPayloadSize,
      unacknowledgedPacketThreshold,
      handshakeTimeout,
      maxStreamWindow,
      maxWindow,
      rxDiagnosticLoss,
      txDiagnosticLoss,
      udpReceiveBufferSize,
      udpSendBufferSize,
      udpTTL,
      noUdpPayloadSizeShaping,
      validateAddress,
      disableActiveMigration,
      ipv6Only,
      cc,
      resetTokenSecret,
      tokenSecret,
    } = options;

    // All of the other options will be validated internally by the C++ code
    if (address !== undefined && !SocketAddress.isSocketAddress(address)) {
      if (typeof address === 'object' && address !== null) {
        address = new SocketAddress(address);
      } else {
        throw new ERR_INVALID_ARG_TYPE('options.address', 'SocketAddress', address);
      }
    }

    return {
      __proto__: null,
      address: address?.[kSocketAddressHandle],
      retryTokenExpiration,
      tokenExpiration,
      maxConnectionsPerHost,
      maxConnectionsTotal,
      maxStatelessResetsPerHost,
      addressLRUSize,
      maxRetries,
      maxPayloadSize,
      unacknowledgedPacketThreshold,
      handshakeTimeout,
      maxStreamWindow,
      maxWindow,
      rxDiagnosticLoss,
      txDiagnosticLoss,
      udpReceiveBufferSize,
      udpSendBufferSize,
      udpTTL,
      noUdpPayloadSizeShaping,
      validateAddress,
      disableActiveMigration,
      ipv6Only,
      cc,
      resetTokenSecret,
      tokenSecret,
    };
  }

  #newSession(handle) {
    const session = new QuicSession(kPrivateConstructor, this.#sessionConfig, handle, this);
    this.#sessions.add(session);
    return session;
  }

  /**
   * @param {EndpointCallbackConfiguration} config
   */
  constructor(config = kEmptyObject) {
    const {
      onsession,
      session,
    } = this.#processEndpointConfig(config);

    // Note that the onsession callback is only used for server sessions.
    // If the callback is not specified, calling listen() will fail but
    // connect() can still be called.
    if (onsession !== undefined) {
      this.#onsession = onsession.bind(this);
    }
    this.#sessionConfig = session;

    this.#handle = new Endpoint_(this.#processEndpointOptions(config));
    this.#handle[kOwner] = this;
    this.#stats = new QuicEndpointStats(kPrivateConstructor, this.#handle.stats);
    this.#state = new QuicEndpointState(kPrivateConstructor, this.#handle.state);

    if (onEndpointCreatedChannel.hasSubscribers) {
      onEndpointCreatedChannel.publish({
        endpoint: this,
        config,
      });
    }
  }

  /** @type {QuicEndpointStats} */
  get stats() { return this.#stats; }

  /** @type {QuicEndpointState} */
  get state() { return this.#state; }

  get #isClosedOrClosing() {
    return this.#handle === undefined || this.#isPendingClose;
  }

  /**
   * When an endpoint is marked as busy, it will not accept new connections.
   * Existing connections will continue to work.
   * @type {boolean}
   */
  get busy() { return this.#busy; }

  /**
   * @type {boolean}
   */
  set busy(val) {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Endpoint is closed');
    }
    // The val is allowed to be any truthy value
    // Non-op if there is no change
    if (!!val !== this.#busy) {
      this.#busy = !this.#busy;
      this.#handle.markBusy(this.#busy);
      if (onEndpointBusyChangeChannel.hasSubscribers) {
        onEndpointBusyChangeChannel.publish({
          endpoint: this,
          busy: this.#busy,
        });
      }
    }
  }

  /**
   * The local address the endpoint is bound to (if any)
   * @type {SocketAddress|undefined}
   */
  get address() {
    if (this.#isClosedOrClosing) return undefined;
    if (this.#address === undefined) {
      const addr = this.#handle.address();
      if (addr !== undefined) this.#address = new InternalSocketAddress(addr);
    }
    return this.#address;
  }

  /**
   * @param {TlsOptions} tls
   */
  #processTlsOptions(tls) {
    validateObject(tls, 'options.tls');
    const {
      sni,
      alpn,
      ciphers = DEFAULT_CIPHERS,
      groups = DEFAULT_GROUPS,
      keylog = false,
      verifyClient = false,
      tlsTrace = false,
      verifyPrivateKey = false,
      keys,
      certs,
      ca,
      crl,
    } = tls;

    if (sni !== undefined) {
      validateString(sni, 'options.tls.sni');
    }
    if (alpn !== undefined) {
      validateString(alpn, 'options.tls.alpn');
    }
    if (ciphers !== undefined) {
      validateString(ciphers, 'options.tls.ciphers');
    }
    if (groups !== undefined) {
      validateString(groups, 'options.tls.groups');
    }
    validateBoolean(keylog, 'options.tls.keylog');
    validateBoolean(verifyClient, 'options.tls.verifyClient');
    validateBoolean(tlsTrace, 'options.tls.tlsTrace');
    validateBoolean(verifyPrivateKey, 'options.tls.verifyPrivateKey');

    if (certs !== undefined) {
      const certInputs = ArrayIsArray(certs) ? certs : [certs];
      for (const cert of certInputs) {
        if (!isArrayBufferView(cert) && !isArrayBuffer(cert)) {
          throw new ERR_INVALID_ARG_TYPE('options.tls.certs',
                                         ['ArrayBufferView', 'ArrayBuffer'], cert);
        }
      }
    }

    if (ca !== undefined) {
      const caInputs = ArrayIsArray(ca) ? ca : [ca];
      for (const caCert of caInputs) {
        if (!isArrayBufferView(caCert) && !isArrayBuffer(caCert)) {
          throw new ERR_INVALID_ARG_TYPE('options.tls.ca',
                                         ['ArrayBufferView', 'ArrayBuffer'], caCert);
        }
      }
    }

    if (crl !== undefined) {
      const crlInputs = ArrayIsArray(crl) ? crl : [crl];
      for (const crlCert of crlInputs) {
        if (!isArrayBufferView(crlCert) && !isArrayBuffer(crlCert)) {
          throw new ERR_INVALID_ARG_TYPE('options.tls.crl',
                                         ['ArrayBufferView', 'ArrayBuffer'], crlCert);
        }
      }
    }

    const keyHandles = [];
    if (keys !== undefined) {
      const keyInputs = ArrayIsArray(keys) ? keys : [keys];
      for (const key of keyInputs) {
        if (isKeyObject(key)) {
          if (key.type !== 'private') {
            throw new ERR_INVALID_ARG_VALUE('options.tls.keys', key, 'must be a private key');
          }
          ArrayPrototypePush(keyHandles, key[kKeyObjectHandle]);
        } else if (isCryptoKey(key)) {
          if (key.type !== 'private') {
            throw new ERR_INVALID_ARG_VALUE('options.tls.keys', key, 'must be a private key');
          }
          ArrayPrototypePush(keyHandles, key[kKeyObjectInner][kKeyObjectHandle]);
        } else {
          throw new ERR_INVALID_ARG_TYPE('options.tls.keys', ['KeyObject', 'CryptoKey'], key);
        }
      }
    }

    return {
      __proto__: null,
      sni,
      alpn,
      ciphers,
      groups,
      keylog,
      verifyClient,
      tlsTrace,
      verifyPrivateKey,
      keys: keyHandles,
      certs,
      ca,
      crl,
    };
  }

  /**
   * @param {'use'|'ignore'|'default'} policy
   * @returns {number}
   */
  #getPreferredAddressPolicy(policy = 'default') {
    switch (policy) {
      case 'use': return PREFERRED_ADDRESS_USE;
      case 'ignore': return PREFERRED_ADDRESS_IGNORE;
      case 'default': return DEFAULT_PREFERRED_ADDRESS_POLICY;
    }
    throw new ERR_INVALID_ARG_VALUE('options.preferredAddressPolicy', policy);
  }

  /**
   * @param {SessionOptions} options
   */
  #processSessionOptions(options) {
    validateObject(options, 'options');
    const {
      version,
      minVersion,
      preferredAddressPolicy = 'default',
      application = kEmptyObject,
      transportParams = kEmptyObject,
      tls = kEmptyObject,
      qlog = false,
      sessionTicket,
    } = options;

    return {
      __proto__: null,
      version,
      minVersion,
      preferredAddressPolicy: this.#getPreferredAddressPolicy(preferredAddressPolicy),
      application,
      transportParams,
      tls: this.#processTlsOptions(tls),
      qlog,
      sessionTicket,
    };
  }

  /**
   * Configures the endpoint to listen for incoming connections.
   * @param {SessionOptions} [options]
   */
  listen(options = kEmptyObject) {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Endpoint is closed');
    }
    if (this.#onsession === undefined) {
      throw new ERR_INVALID_STATE(
        'Endpoint is not configured to accept sessions. Specify an onsession ' +
        'callback when creating the endpoint',
      );
    }
    if (this.#listening) {
      throw new ERR_INVALID_STATE('Endpoint is already listening');
    }
    this.#handle.listen(this.#processSessionOptions(options));
    this.#listening = true;

    if (onEndpointListeningChannel.hasSubscribers) {
      onEndpointListeningChannel.publish({
        endpoint: this,
        options,
      });
    }
  }

  /**
   * Initiates a session with a remote endpoint.
   * @param {SocketAddress} address
   * @param {SessionOptions} [options]
   * @returns {QuicSession}
   */
  connect(address, options = kEmptyObject) {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Endpoint is closed');
    }

    if (!SocketAddress.isSocketAddress(address)) {
      if (address == null || typeof address !== 'object') {
        throw new ERR_INVALID_ARG_TYPE('address', 'SocketAddress', address);
      }
      address = new SocketAddress(address);
    }

    const processedOptions = this.#processSessionOptions(options);
    const { sessionTicket } = processedOptions;

    const handle = this.#handle.connect(address[kSocketAddressHandle],
                                        processedOptions, sessionTicket);

    if (handle === undefined) {
      throw new ERR_QUIC_CONNECTION_FAILED();
    }
    const session = this.#newSession(handle);

    if (onEndpointClientSessionChannel.hasSubscribers) {
      onEndpointClientSessionChannel.publish({
        endpoint: this,
        session,
        address,
        options,
      });
    }

    return session;
  }

  /**
   * Gracefully closes the endpoint. Any existing sessions will be permitted to
   * end gracefully, after which the endpoint will be closed. New sessions will
   * not be accepted or created. The returned promise will be resolved when
   * closing is complete, or will be rejected if the endpoint is closed abruptly
   * due to an error.
   * @returns {Promise<void>} Returns this.closed
   */
  close() {
    if (!this.#isClosedOrClosing) {
      if (onEndpointClosingChannel.hasSubscribers) {
        onEndpointClosingChannel.publish({
          endpoint: this,
          hasPendingError: this.#pendingError !== undefined,
        });
      }
      this.#isPendingClose = true;
      this.#handle?.closeGracefully();
    }
    return this.closed;
  }

  /**
   * Returns a promise that is resolved when the endpoint is closed or rejects
   * if the endpoint is closed abruptly due to an error. The closed property
   * is set to the same promise that is returned by the close() method.
   * @type {Promise<void>}
   */
  get closed() { return this.#pendingClose.promise; }

  /** @type {boolean} */
  get destroyed() { return this.#handle === undefined; }

  /**
   * Return an iterator over all currently active sessions associated
   * with this endpoint.
   * @type {SetIterator<QuicSession>}
   */
  get sessions() {
    return this.#sessions[SymbolIterator]();
  }

  /**
   * Forcefully terminates the endpoint by immediately destroying all sessions
   * after calling close. If an error is given, the closed promise will be
   * rejected with that error. If no error is given, the closed promise will
   * be resolved.
   * @param {any} [error]
   * @returns {Promise<void>} Returns this.closed
   */
  destroy(error) {
    if (!this.#isClosedOrClosing) {
      // Start closing the endpoint.
      this.#pendingError = error;
      // Trigger a graceful close of the endpoint that'll ensure that the
      // endpoint is closed down after all sessions are closed...
      this.close();
    }
    // Now, force all sessions to be abruptly closed...
    for (const session of this.#sessions) {
      session.destroy(error);
    }
    return this.closed;
  }

  ref() {
    if (this.#handle !== undefined) this.#handle.ref(true);
  }

  unref() {
    if (this.#handle !== undefined) this.#handle.ref(false);
  }

  #maybeGetCloseError(context, status) {
    switch (context) {
      case kCloseContextClose: {
        return this.#pendingError;
      }
      case kCloseContextBindFailure: {
        return new ERR_QUIC_ENDPOINT_CLOSED('Bind failure', status);
      }
      case kCloseContextListenFailure: {
        return new ERR_QUIC_ENDPOINT_CLOSED('Listen failure', status);
      }
      case kCloseContextReceiveFailure: {
        return new ERR_QUIC_ENDPOINT_CLOSED('Receive failure', status);
      }
      case kCloseContextSendFailure: {
        return new ERR_QUIC_ENDPOINT_CLOSED('Send failure', status);
      }
      case kCloseContextStartFailure: {
        return new ERR_QUIC_ENDPOINT_CLOSED('Start failure', status);
      }
    }
    // Otherwise return undefined.
  }

  [kFinishClose](context, status) {
    if (this.#handle === undefined) return;
    this.#handle = undefined;
    this.#stats[kFinishClose]();
    this.#state[kFinishClose]();
    this.#address = undefined;
    this.#busy = false;
    this.#listening = false;
    this.#isPendingClose = false;

    // As QuicSessions are closed they are expected to remove themselves
    // from the sessions collection. Just in case they don't, let's force
    // it by resetting the set so we don't leak memory. Let's emit a warning,
    // tho, if the set is not empty at this point as that would indicate a
    // bug in Node.js that should be fixed.
    if (this.#sessions.size > 0) {
      process.emitWarning(
        `The endpoint is closed with ${this.#sessions.size} active sessions. ` +
        'This should not happen and indicates a bug in Node.js. Please open an ' +
        'issue in the Node.js GitHub repository at https://github.com/nodejs/node ' +
        'to report the problem.',
      );
    }
    this.#sessions.clear();

    // If destroy was called with an error, then the this.#pendingError will be
    // set. Or, if context indicates an error condition that caused the endpoint
    // to be closed, the status will indicate the error code. In either case,
    // we will reject the pending close promise at this point.
    const maybeCloseError = this.#maybeGetCloseError(context, status);
    if (maybeCloseError !== undefined) {
      if (onEndpointErrorChannel.hasSubscribers) {
        onEndpointErrorChannel.publish({
          endpoint: this,
          error: maybeCloseError,
        });
      }
      this.#pendingClose.reject(maybeCloseError);
    } else {
      // Otherwise we are good to resolve the pending close promise!
      this.#pendingClose.resolve();
    }
    if (onEndpointClosedChannel.hasSubscribers) {
      onEndpointClosedChannel.publish({
        endpoint: this,
      });
    }

    // Note that we are intentionally not clearing the
    // this.#pendingClose.promise here.
    this.#pendingClose.resolve = undefined;
    this.#pendingClose.reject = undefined;
    this.#pendingError = undefined;
  }

  [kNewSession](handle) {
    const session = this.#newSession(handle);
    if (onEndpointServerSessionChannel.hasSubscribers) {
      onEndpointServerSessionChannel.publish({
        endpoint: this,
        session,
      });
    }
    this.#onsession(session);
  }

  // Called by the QuicSession when it closes to remove itself from
  // the active sessions tracked by the QuicEndpoint.
  [kRemoveSession](session) {
    this.#sessions.delete(session);
  }

  async [SymbolAsyncDispose]() { await this.close(); }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `QuicEndpoint ${inspect({
      address: this.address,
      busy: this.busy,
      closed: this.closed,
      closing: this.#isPendingClose,
      destroyed: this.destroyed,
      listening: this.#listening,
      sessions: this.#sessions,
      stats: this.stats,
      state: this.state,
    }, opts)}`;
  }
};

function readOnlyConstant(value) {
  return {
    __proto__: null,
    value,
    writable: false,
    configurable: false,
    enumerable: true,
  };
}

ObjectDefineProperties(QuicEndpoint, {
  CC_ALGO_RENO: readOnlyConstant(CC_ALGO_RENO),
  CC_ALGO_CUBIC: readOnlyConstant(CC_ALGO_CUBIC),
  CC_ALGO_BBR: readOnlyConstant(CC_ALGO_BBR),
  CC_ALGP_RENO_STR: readOnlyConstant(CC_ALGO_RENO_STR),
  CC_ALGO_CUBIC_STR: readOnlyConstant(CC_ALGO_CUBIC_STR),
  CC_ALGO_BBR_STR: readOnlyConstant(CC_ALGO_BBR_STR),
});
ObjectDefineProperties(QuicSession, {
  DEFAULT_CIPHERS: readOnlyConstant(DEFAULT_CIPHERS),
  DEFAULT_GROUPS: readOnlyConstant(DEFAULT_GROUPS),
});

module.exports = {
  QuicEndpoint,
  QuicSession,
  QuicStream,
  QuicSessionState,
  QuicSessionStats,
  QuicStreamState,
  QuicStreamStats,
  QuicEndpointState,
  QuicEndpointStats,
};

/* c8 ignore stop */

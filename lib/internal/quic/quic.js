'use strict';

// TODO(@jasnell) Temporarily ignoring c8 covrerage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  ArrayIsArray,
  ArrayPrototypePush,
  BigInt,
  DataViewPrototypeGetByteLength,
  FunctionPrototypeBind,
  Number,
  ObjectDefineProperties,
  ObjectKeys,
  PromisePrototypeThen,
  PromiseResolve,
  PromiseWithResolvers,
  SafeSet,
  Symbol,
  SymbolAsyncDispose,
  SymbolAsyncIterator,
  SymbolDispose,
  SymbolIterator,
  TypedArrayPrototypeGetByteLength,
  Uint8Array,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

// QUIC requires that Node.js be compiled with crypto support.
if (!process.features.quic || !getOptionValue('--experimental-quic')) {
  return;
}

const { inspect } = require('internal/util/inspect');

let debug = require('internal/util/debuglog').debuglog('quic', (fn) => {
  debug = fn;
});

const {
  Endpoint: Endpoint_,
  setCallbacks,

  // The constants to be exposed to end users for various options.
  CC_ALGO_RENO_STR: CC_ALGO_RENO,
  CC_ALGO_CUBIC_STR: CC_ALGO_CUBIC,
  CC_ALGO_BBR_STR: CC_ALGO_BBR,
  DEFAULT_CIPHERS,
  DEFAULT_GROUPS,

  // Internal constants for use by the implementation.
  // These are not exposed to end users.
  PREFERRED_ADDRESS_IGNORE: kPreferredAddressIgnore,
  PREFERRED_ADDRESS_USE: kPreferredAddressUse,
  DEFAULT_PREFERRED_ADDRESS_POLICY: kPreferredAddressDefault,
  STREAM_DIRECTION_BIDIRECTIONAL: kStreamDirectionBidirectional,
  STREAM_DIRECTION_UNIDIRECTIONAL: kStreamDirectionUnidirectional,
  CLOSECONTEXT_CLOSE: kCloseContextClose,
  CLOSECONTEXT_BIND_FAILURE: kCloseContextBindFailure,
  CLOSECONTEXT_LISTEN_FAILURE: kCloseContextListenFailure,
  CLOSECONTEXT_RECEIVE_FAILURE: kCloseContextReceiveFailure,
  CLOSECONTEXT_SEND_FAILURE: kCloseContextSendFailure,
  CLOSECONTEXT_START_FAILURE: kCloseContextStartFailure,
  QUIC_STREAM_HEADERS_KIND_INITIAL: kHeadersKindInitial,
  QUIC_STREAM_HEADERS_KIND_HINTS: kHeadersKindHints,
  QUIC_STREAM_HEADERS_KIND_TRAILING: kHeadersKindTrailing,
  QUIC_STREAM_HEADERS_FLAGS_NONE: kHeadersFlagsNone,
  QUIC_STREAM_HEADERS_FLAGS_TERMINAL: kHeadersFlagsTerminal,
} = internalBinding('quic');

// Maps the numeric HeadersKind constants from C++ to user-facing strings.
// Indexed by the enum value (HINTS=0, INITIAL=1, TRAILING=2).
const kHeadersKindName = [];
kHeadersKindName[kHeadersKindHints] = 'hints';
kHeadersKindName[kHeadersKindInitial] = 'initial';
kHeadersKindName[kHeadersKindTrailing] = 'trailing';

const {
  markPromiseAsHandled,
} = internalBinding('util');

const {
  isArrayBuffer,
  isArrayBufferView,
  isDataView,
  isPromise,
  isSharedArrayBuffer,
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
    ERR_INVALID_THIS,
    ERR_MISSING_ARGS,
    ERR_OUT_OF_RANGE,
    ERR_QUIC_APPLICATION_ERROR,
    ERR_QUIC_CONNECTION_FAILED,
    ERR_QUIC_ENDPOINT_CLOSED,
    ERR_QUIC_OPEN_STREAM_FAILED,
    ERR_QUIC_STREAM_ABORTED,
    ERR_QUIC_STREAM_RESET,
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
  createBlobReaderIterable,
  isBlob,
  kHandle: kBlobHandle,
} = require('internal/blob');

const {
  drainableProtocol,
  kValidatedSource,
} = require('internal/streams/iter/types');

const {
  toUint8Array,
  convertChunks,
} = require('internal/streams/iter/utils');

const {
  from: streamFrom,
  fromSync: streamFromSync,
} = require('internal/streams/iter/from');

const {
  isKeyObject,
} = require('internal/crypto/keys');

const {
  FileHandle,
  kHandle: kFileHandle,
  kLocked: kFileLocked,
} = require('internal/fs/promises');

const {
  validateAbortSignal,
  validateBoolean,
  validateFunction,
  validateInteger,
  validateObject,
  validateOneOf,
  validateString,
} = require('internal/validators');

const {
  buildNgHeaderString,
  assertValidPseudoHeader,
} = require('internal/http2/util');

const kEmptyObject = { __proto__: null };

const {
  kAttachFileHandle,
  kBlocked,
  kConnect,
  kDatagram,
  kDatagramStatus,
  kDrain,
  kEarlyDataRejected,
  kFinishClose,
  kGoaway,
  kHandshake,
  kHandshakeCompleted,
  kHeaders,
  kOwner,
  kRemoveSession,
  kKeylog,
  kListen,
  kNewSession,
  kQlog,
  kRemoveStream,
  kNewStream,
  kNewToken,
  kOrigin,
  kStreamCallbacks,
  kPathValidation,
  kPrivateConstructor,
  kReset,
  kSendHeaders,
  kSessionTicket,
  kTrailers,
  kVersionNegotiation,
  kInspect,
  kKeyObjectHandle,
  kWantsHeaders,
  kWantsTrailers,
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

const assert = require('internal/assert');

const {
  hasObserver,
  startPerf,
  stopPerf,
} = require('internal/perf/observe');

const kPerfEntry = Symbol('kPerfEntry');

const {
  onEndpointCreatedChannel,
  onEndpointListeningChannel,
  onEndpointClosingChannel,
  onEndpointClosedChannel,
  onEndpointErrorChannel,
  onEndpointBusyChangeChannel,
  onEndpointClientSessionChannel,
  onEndpointServerSessionChannel,
  onSessionOpenStreamChannel,
  onSessionReceivedStreamChannel,
  onSessionSendDatagramChannel,
  onSessionUpdateKeyChannel,
  onSessionClosingChannel,
  onSessionClosedChannel,
  onSessionReceiveDatagramChannel,
  onSessionReceiveDatagramStatusChannel,
  onSessionPathValidationChannel,
  onSessionNewTokenChannel,
  onSessionTicketChannel,
  onSessionVersionNegotiationChannel,
  onSessionOriginChannel,
  onSessionHandshakeChannel,
  onSessionGoawayChannel,
  onSessionEarlyRejectedChannel,
  onStreamClosedChannel,
  onStreamHeadersChannel,
  onStreamTrailersChannel,
  onStreamInfoChannel,
  onStreamResetChannel,
  onStreamBlockedChannel,
  onSessionErrorChannel,
  onEndpointConnectChannel,
} = require('internal/quic/diagnostics');

const kNilDatagramId = 0n;

// Module-level registry of all live QuicEndpoint instances. Used by
// connect() and listen() to find existing endpoints for reuse instead
// of creating a new one per session.
const endpointRegistry = new SafeSet();

/**
 * @typedef {import('../socketaddress.js').SocketAddress} SocketAddress
 * @typedef {import('../crypto/keys.js').KeyObject} KeyObject
 */

/**
 * @typedef {object} OpenStreamOptions
 * @property {string|ArrayBuffer|SharedArrayBuffer|ArrayBufferView|Blob|
 *   FileHandle|AsyncIterable|Iterable|Promise|null} [body] The outbound
 *   body source. See the public docs for `stream.setBody()` for details
 *   on supported types. When omitted, the stream is closed immediately.
 * @property {object} [headers] Initial request or response headers to
 *   send. Only used when the negotiated application supports headers
 *   (e.g. HTTP/3).
 * @property {'high'|'default'|'low'} [priority] The priority level of the stream.
 * @property {boolean} [incremental] Whether to interleave data with same-priority streams.
 * @property {number} [highWaterMark] The high water mark for write
 *   backpressure, in bytes. **Default:** `65536`.
 * @property {OnHeadersCallback} [onheaders] Callback for incoming initial headers
 * @property {OnTrailersCallback} [ontrailers] Callback for incoming trailing headers
 * @property {OnInfoCallback} [oninfo] Callback for informational (1xx) headers
 * @property {OnWantTrailersCallback} [onwanttrailers] Callback fired when the
 *   transport is ready to send trailers for this stream.
 */

/**
 * Provides the configuration options for a QuicEndpoint.
 * @typedef {object} EndpointOptions
 * @property {SocketAddress|string} [address] The local address to bind to
 * @property {bigint|number} [addressLRUSize] The size of the address LRU cache
 * @property {'reno'|'cubic'|'bbr'} [cc] The congestion control algorithm
 * @property {boolean} [disableStatelessReset] When true, the endpoint will not send stateless resets
 * @property {bigint|number} [idleTimeout] The default idle timeout for sessions on this endpoint
 * @property {boolean} [ipv6Only] Use IPv6 only
 * @property {bigint|number} [maxConnectionsPerHost] The maximum number of connections per host
 * @property {bigint|number} [maxConnectionsTotal] The maximum number of total connections
 * @property {bigint|number} [maxRetries] The maximum number of retries
 * @property {bigint|number} [maxStatelessResetsPerHost] The maximum number of stateless resets per host
 * @property {ArrayBufferView} [resetTokenSecret] The reset token secret
 * @property {bigint|number} [retryTokenExpiration] The retry token expiration
 * @property {number} [rxDiagnosticLoss] The receive diagnostic loss probability (range 0.0-1.0)
 * @property {bigint|number} [tokenExpiration] The token expiration
 * @property {ArrayBufferView} [tokenSecret] The token secret
 * @property {number} [txDiagnosticLoss] The transmit diagnostic loss probability (range 0.0-1.0)
 * @property {number} [udpReceiveBufferSize] The UDP receive buffer size
 * @property {number} [udpSendBufferSize] The UDP send buffer size
 * @property {number} [udpTTL] The UDP TTL
 * @property {boolean} [validateAddress] Validate the address using retry packets
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
 * Per-identity TLS options. Used as the values in the `sni` map of
 * `SessionOptions` for server endpoints.
 * @typedef {object} IdentityOptions
 * @property {KeyObject|KeyObject[]} keys The TLS private keys.
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} certs The TLS certificates.
 * @property {boolean} [verifyPrivateKey] Verify the private key.
 *   **Default:** `false`.
 * @property {number} [port] The port to advertise in HTTP/3 ORIGIN frames
 *   for this host name. **Default:** `443`.
 * @property {boolean} [authoritative] Whether to include this host name
 *   in HTTP/3 ORIGIN frames. **Default:** `true`. Wildcard (`'*'`)
 *   entries are always excluded regardless of this setting.
 */

/**
 * @typedef {object} SessionOptions
 * @property {EndpointOptions|QuicEndpoint} [endpoint] An endpoint to use.
 * @property {boolean} [reuseEndpoint] When `true` (default), `connect()`
 *   will attempt to reuse an existing endpoint rather than create a new
 *   one. Has no effect for server sessions.
 * @property {number} [version] The QUIC version
 * @property {number} [minVersion] The minimum acceptable QUIC version
 * @property {'use'|'ignore'|'default'} [preferredAddressPolicy] The preferred address policy
 * @property {ApplicationOptions} [application] The application options
 * @property {TransportParams} [transportParams] The transport parameters
 * @property {string} [servername] The server name identifier (client only)
 * @property {string|string[]} [alpn] The ALPN protocol identifier(s).
 *   For client sessions, a single string. For server sessions, an array
 *   of protocol names in preference order.
 * @property {string} [ciphers] The TLS ciphers
 * @property {string} [groups] The TLS key-exchange groups
 * @property {boolean} [keylog] Enable TLS key logging
 * @property {boolean} [verifyClient] Verify the client certificate (server only)
 * @property {boolean} [tlsTrace] Enable TLS tracing
 * @property {boolean} [enableEarlyData] Enable 0-RTT early data.
 *   **Default:** `true`.
 * @property {boolean} [rejectUnauthorized] Verify the peer certificate
 *   against the supplied CAs. **Default:** `true`.
 * @property {boolean} [verifyPrivateKey] Verify the private key (client only)
 * @property {KeyObject|KeyObject[]} [keys] The TLS private keys (client only)
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [certs] The TLS certificates (client only)
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [ca] The certificate authority
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [crl] The certificate revocation list
 * @property {{[key: string]: IdentityOptions}} [sni] Map of host names to
 *   per-identity TLS options for Server Name Indication. Required for
 *   server sessions. The special key `'*'` specifies the optional
 *   default/fallback identity.
 * @property {boolean} [qlog] Enable qlog
 * @property {ArrayBufferView} [sessionTicket] A session ticket from a
 *   prior session, used to resume that session (client only).
 * @property {ArrayBufferView} [token] An opaque address validation token
 *   previously received from the server via `onnewtoken` (client only).
 * @property {bigint|number} [handshakeTimeout] The handshake timeout
 * @property {bigint|number} [keepAlive] The keep-alive timeout in milliseconds. When set,
 *   PING frames will be sent automatically to prevent idle timeout.
 * @property {bigint|number} [maxStreamWindow] The maximum stream window
 * @property {bigint|number} [maxWindow] The maximum connection window
 * @property {bigint|number} [maxPayloadSize] The maximum payload size
 * @property {bigint|number} [unacknowledgedPacketThreshold] The unacknowledged packet threshold
 * @property {'reno'|'cubic'|'bbr'} [cc] The congestion control algorithm
 * @property {'drop-oldest'|'drop-newest'} [datagramDropPolicy] The
 *   policy used when the pending datagram queue is full.
 *   **Default:** `'drop-oldest'`.
 * @property {number} [drainingPeriodMultiplier] Multiplier applied to the
 *   draining period (3 * PTO) used by ngtcp2. Range `3..255`.
 *   **Default:** `3`.
 * @property {number} [maxDatagramSendAttempts] Maximum number of times a
 *   datagram is retried before being abandoned. Range `1..255`.
 *   **Default:** `5`.
 * @property {OnSessionErrorCallback} [onerror] Session error callback.
 * @property {OnStreamCallback} [onstream] Incoming stream callback.
 * @property {OnDatagramCallback} [ondatagram] Incoming datagram callback.
 * @property {OnDatagramStatusCallback} [ondatagramstatus] Outgoing datagram status callback.
 * @property {OnPathValidationCallback} [onpathvalidation] Path validation callback.
 * @property {OnSessionTicketCallback} [onsessionticket] New session-ticket callback.
 * @property {OnVersionNegotiationCallback} [onversionnegotiation] Version negotiation callback.
 * @property {OnHandshakeCallback} [onhandshake] Handshake-completed callback.
 * @property {OnNewTokenCallback} [onnewtoken] NEW_TOKEN frame callback (client only).
 * @property {OnOriginCallback} [onorigin] ORIGIN frame callback (client only).
 * @property {OnGoawayCallback} [ongoaway] GOAWAY frame callback.
 * @property {OnKeylogCallback} [onkeylog] TLS key-log callback.
 * @property {OnQlogCallback} [onqlog] qlog data callback.
 * @property {OnHeadersCallback} [onheaders] Default per-stream initial-headers callback.
 * @property {OnTrailersCallback} [ontrailers] Default per-stream trailing-headers callback.
 * @property {OnInfoCallback} [oninfo] Default per-stream informational-headers callback.
 * @property {OnWantTrailersCallback} [onwanttrailers] Default per-stream
 *   want-trailers callback.
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
 * @typedef {object} QuicSessionInfo
 * @property {SocketAddress} local The local address
 * @property {SocketAddress} remote The remote address
 * @property {string} protocol The alpn protocol identifier negotiated for this session
 * @property {string} servername The servername identifier for this session
 * @property {string} cipher The cipher suite negotiated for this session
 * @property {string} cipherVersion The version of the cipher suite negotiated for this session
 * @property {string} [validationErrorReason] The reason the session failed validation (if any)
 * @property {string} [validationErrorCode] The error code for the validation failure (if any)
 */

/**
 * Called when the Endpoint receives a new server-side Session.
 * @callback OnSessionCallback
 * @this {QuicEndpoint}
 * @param {QuicSession} session
 * @returns {void}
 */

/**
 * Called when a session is destroyed with an error.
 * @callback OnSessionErrorCallback
 * @this {QuicSession}
 * @param {any} error
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
 * Called when the status of a previously sent datagram is reported.
 * @callback OnDatagramStatusCallback
 * @this {QuicSession}
 * @param {bigint} id The datagram id
 * @param {'acknowledged'|'lost'|'abandoned'} status
 * @returns {void}
 */

/**
 * Called when QUIC path validation completes (or fails).
 * @callback OnPathValidationCallback
 * @this {QuicSession}
 * @param {'success'|'failure'|'aborted'} result
 * @param {SocketAddress} newLocalAddress
 * @param {SocketAddress} newRemoteAddress
 * @param {SocketAddress|null} oldLocalAddress
 * @param {SocketAddress|null} oldRemoteAddress
 * @param {boolean} [preferredAddress] `true` if the validation was triggered
 *   by a preferred-address migration on the client side.
 * @returns {void}
 */

/**
 * @callback OnSessionTicketCallback
 * @this {QuicSession}
 * @param {object} ticket
 * @returns {void}
 */

/**
 * Called when the server responds with a Version Negotiation packet.
 * The session is destroyed immediately after this returns.
 * @callback OnVersionNegotiationCallback
 * @this {QuicSession}
 * @param {number} version The QUIC version configured for this session
 * @param {number[]} requestedVersions The versions advertised by the server
 * @param {number[]} supportedVersions A `[minVersion, maxVersion]` pair
 * @returns {void}
 */

/**
 * Called when the TLS handshake completes successfully.
 * @callback OnHandshakeCallback
 * @this {QuicSession}
 * @param {string} sni
 * @param {string} alpn
 * @param {string} cipher
 * @param {string} cipherVersion
 * @param {string} [validationErrorReason]
 * @param {number} [validationErrorCode]
 * @param {boolean} earlyDataAttempted
 * @param {boolean} earlyDataAccepted
 * @returns {void}
 */

/**
 * Called when the server issues a NEW_TOKEN frame to the client.
 * @callback OnNewTokenCallback
 * @this {QuicSession}
 * @param {Buffer} token The opaque token data
 * @param {SocketAddress} address The remote server address
 * @returns {void}
 */

/**
 * Called when the server sends an ORIGIN frame.
 * @callback OnOriginCallback
 * @this {QuicSession}
 * @param {string[]} origins The list of origins the server claims authority for
 * @returns {void}
 */

/**
 * Called when the peer sends a GOAWAY frame (HTTP/3 only).
 * @callback OnGoawayCallback
 * @this {QuicSession}
 * @param {bigint} lastStreamId The highest stream ID the peer may have processed
 * @returns {void}
 */

/**
 * Called when TLS key-log material is available. Only fires when
 * `sessionOptions.keylog` is `true`.
 * @callback OnKeylogCallback
 * @this {QuicSession}
 * @param {string} line A single NSS Key Log Format line, including trailing newline.
 * @returns {void}
 */

/**
 * Called when qlog diagnostic data is available. Only fires when
 * `sessionOptions.qlog` is `true`.
 * @callback OnQlogCallback
 * @this {QuicSession}
 * @param {string} data A chunk of JSON-SEQ formatted qlog data
 * @param {boolean} fin `true` if this is the final qlog chunk for the session.
 * @returns {void}
 */

/**
 * @callback OnBlockedCallback
 * @this {QuicStream}
 * @returns {void}
 */

/**
 * @callback OnStreamErrorCallback
 * @this {QuicStream}
 * @param {any} error
 * @returns {void}
 */

/**
 * Called when initial request or response headers are received.
 * @callback OnHeadersCallback
 * @this {QuicStream}
 * @param {object} headers Header object with lowercase string keys and
 *   string or string-array values.
 * @returns {void}
 */

/**
 * Called when trailing headers are received from the peer.
 * @callback OnTrailersCallback
 * @this {QuicStream}
 * @param {object} trailers Trailing header object.
 * @returns {void}
 */

/**
 * Called when informational (1xx) headers are received from the server
 * (e.g. 103 Early Hints).
 * @callback OnInfoCallback
 * @this {QuicStream}
 * @param {object} headers Informational header object.
 * @returns {void}
 */

/**
 * Called when the transport is ready to send trailers for this stream.
 * The handler should call `stream.sendTrailers(...)` (or
 * `stream.sendTrailers()` with previously-set trailers) to provide them.
 * @callback OnWantTrailersCallback
 * @this {QuicStream}
 * @returns {void}
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
    debug('endpoint close callback', status);
    this[kOwner][kFinishClose](context, status);
  },
  /**
   * Called when the QuicEndpoint C++ handle receives a new server-side session
   * @param {*} session The QuicSession C++ handle
   */
  onSessionNew(session) {
    debug('new server session callback', this[kOwner], session);
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
    debug('session close callback', errorType, code, reason);
    this[kOwner][kFinishClose](errorType, code, reason);
  },

  /**
   * Called when the peer sends a GOAWAY frame (HTTP/3 only).
   * @param {bigint} lastStreamId The highest stream ID the peer may have
   *   processed. Streams above this ID were not processed and can be retried.
   */
  onSessionGoaway(lastStreamId) {
    debug('session goaway callback', lastStreamId);
    this[kOwner][kGoaway](lastStreamId);
  },

  /**
   * Called when a datagram is received on this session.
   * @param {Uint8Array} uint8Array
   * @param {boolean} early
   */
  onSessionDatagram(uint8Array, early) {
    debug('session datagram callback', TypedArrayPrototypeGetByteLength(uint8Array), early);
    this[kOwner][kDatagram](uint8Array, early);
  },

  /**
   * Called when the status of a datagram is received.
   * @param {bigint} id
   * @param {'lost' | 'acknowledged'} status
   */
  onSessionDatagramStatus(id, status) {
    debug('session datagram status callback', id, status);
    this[kOwner][kDatagramStatus](id, status);
  },

  /**
   * Called when the session handshake completes.
   * @param {string} servername
   * @param {string} protocol
   * @param {string} cipher
   * @param {string} cipherVersion
   * @param {string} validationErrorReason
   * @param {number} validationErrorCode
   * @param {boolean} earlyDataAttempted
   * @param {boolean} earlyDataAccepted
   */
  onSessionHandshake(servername, protocol, cipher, cipherVersion,
                     validationErrorReason,
                     validationErrorCode,
                     earlyDataAttempted,
                     earlyDataAccepted) {
    debug('session handshake callback', servername, protocol, cipher, cipherVersion,
          validationErrorReason, validationErrorCode,
          earlyDataAttempted, earlyDataAccepted);
    this[kOwner][kHandshake](servername, protocol, cipher, cipherVersion,
                             validationErrorReason, validationErrorCode,
                             earlyDataAttempted, earlyDataAccepted);
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
    debug('session path validation callback', this[kOwner]);
    this[kOwner][kPathValidation](result, newLocalAddress, newRemoteAddress,
                                  oldLocalAddress, oldRemoteAddress,
                                  preferredAddress);
  },

  /**
   * Called when the session generates a new TLS session ticket
   * @param {object} ticket An opaque session ticket
   */
  onSessionTicket(ticket) {
    debug('session ticket callback', this[kOwner]);
    this[kOwner][kSessionTicket](ticket);
  },

  /**
   * Called when the client receives a NEW_TOKEN frame from the server.
   * The token can be used for future connections to the same server
   * address to skip address validation.
   * @param {Buffer} token The opaque token data
   * @param {SocketAddress} address The remote server address
   */
  onSessionNewToken(token, address) {
    debug('session new token callback', this[kOwner]);
    this[kOwner][kNewToken](token, address);
  },

  /**
   * Called when the server rejects 0-RTT early data. All streams
   * opened during the 0-RTT phase have been destroyed. The
   * application should re-open streams if needed.
   */
  onSessionEarlyDataRejected() {
    debug('session early data rejected callback', this[kOwner]);
    this[kOwner][kEarlyDataRejected]();
  },

  /**
   * Called when the session receives an ORIGIN frame from the peer (RFC 9412).
   * @param {string[]} origins The list of origins the peer claims authority for
   */
  onSessionOrigin(origins) {
    debug('session origin callback', this[kOwner]);
    this[kOwner][kOrigin](origins);
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
    debug('session version negotiation callback', version, requestedVersions, supportedVersions,
          this[kOwner]);
    this[kOwner][kVersionNegotiation](version, requestedVersions, supportedVersions);
    // Note that immediately following a version negotiation event, the
    // session will be destroyed.
  },

  onSessionKeyLog(line) {
    debug('session key log callback', line, this[kOwner]);
    this[kOwner][kKeylog](line);
  },

  onSessionQlog(data, fin) {
    if (this[kOwner] === undefined) {
      // Qlog data can arrive during ngtcp2_conn creation, before the
      // QuicSession JS wrapper exists. Cache until the wrapper is ready.
      this._pendingQlog ??= [];
      this._pendingQlog.push(data, fin);
      return;
    }
    debug('session qlog callback', this[kOwner]);
    this[kOwner][kQlog](data, fin);
  },

  /**
   * Called when a new stream has been received for the session
   * @param {object} stream The QuicStream C++ handle
   * @param {number} direction The stream direction (0 == bidi, 1 == uni)
   */
  onStreamCreated(stream, direction) {
    const session = this[kOwner];
    // The event is ignored and the stream destroyed if the session has been destroyed.
    debug('stream created callback', session, direction);
    if (session.destroyed) {
      stream.destroy();
      return;
    };
    session[kNewStream](stream, direction);
  },

  // QuicStream callbacks
  onStreamBlocked() {
    debug('stream blocked callback', this[kOwner]);
    // Called when the stream C++ handle has been blocked by flow control.
    this[kOwner][kBlocked]();
  },

  onStreamDrain() {
    // Called when the stream's outbound buffer has capacity for more data.
    debug('stream drain callback', this[kOwner]);
    this[kOwner][kDrain]();
  },

  onStreamClose(error) {
    // Called when the stream C++ handle has been closed. The error is
    // either undefined (clean close) or a raw array [type, code, reason]
    // from QuicError::ToV8Value. Convert to a proper Node.js Error.
    if (error !== undefined) {
      error = convertQuicError(error);
    }
    debug(`stream ${this[kOwner].id} closed callback with error: ${error}`);
    this[kOwner][kFinishClose](error);
  },

  onStreamReset(error) {
    // Called when the stream C++ handle has received a stream reset.
    if (error !== undefined) {
      error = convertQuicError(error);
    }
    debug('stream reset callback', this[kOwner], error);
    this[kOwner][kReset](error);
  },

  onStreamHeaders(headers, kind) {
    // Called when the stream C++ handle has received a full block of headers.
    debug(`stream ${this[kOwner].id} headers callback`, headers, kind);
    this[kOwner][kHeaders](headers, kind);
  },

  onStreamTrailers() {
    // Called when the stream C++ handle is ready to receive trailing headers.
    debug('stream want trailers callback', this[kOwner]);
    this[kOwner][kTrailers]();
  },
});

// QUIC error codes are 62-bit varints (RFC 9000 section 16). The
// maximum representable code is 2**62 - 1.
const kMaxQuicErrorCode = (1n << 62n) - 1n;

/**
 * An Error subclass that carries an explicit numeric QUIC error code.
 * Use this when destroying a stream or aborting an outbound writer to
 * communicate a specific application-protocol-defined error code to
 * the peer. When a `QuicError` is supplied, the QUIC stack uses
 * `errorCode` as the wire code for the resulting RESET_STREAM /
 * STOP_SENDING / CONNECTION_CLOSE frame; otherwise the negotiated
 * application's "internal error" code is used (see
 * `QuicSessionState.internalErrorCode`).
 *
 * The Node.js error code (`error.code`) defaults to
 * `'ERR_QUIC_STREAM_ABORTED'` but can be overridden via
 * `options.code`. The numeric QUIC code lives on the separate
 * `errorCode` property to avoid colliding with Node.js's convention
 * that `error.code` is a string.
 */
class QuicError extends Error {
  /** @type {bigint} */
  #errorCode;
  /** @type {'transport' | 'application'} */
  #type;

  /**
   * @param {string} message
   * @param {object} options
   * @param {bigint|number} options.errorCode The numeric QUIC error
   *   code. Numbers are coerced to BigInt. Must be a non-negative
   *   62-bit unsigned varint
   *   (`0n <= errorCode <= 2n ** 62n - 1n`).
   * @param {string} [options.code] The Node.js-style error code
   *   string assigned to `error.code`. Defaults to
   *   `'ERR_QUIC_STREAM_ABORTED'`.
   * @param {'transport'|'application'} [options.type] Whether the
   *   code is a transport-layer code (defined by RFC 9000) or an
   *   application-layer code (defined by the negotiated ALPN, e.g.
   *   RFC 9114 for HTTP/3). Defaults to `'application'`. Stream
   *   resets always carry application codes; this option is exposed
   *   for use sites that may target either layer.
   */
  constructor(message, options = kEmptyObject) {
    validateString(message, 'message');
    validateObject(options, 'options');
    const {
      errorCode,
      code = 'ERR_QUIC_STREAM_ABORTED',
      type = 'application',
    } = options;
    if (errorCode === undefined) {
      throw new ERR_MISSING_ARGS('options.errorCode');
    }
    if (typeof errorCode !== 'bigint' && typeof errorCode !== 'number') {
      throw new ERR_INVALID_ARG_TYPE('options.errorCode',
                                     ['bigint', 'number'], errorCode);
    }
    validateString(code, 'options.code');
    validateOneOf(type, 'options.type', ['transport', 'application']);
    const numericCode = BigInt(errorCode);
    if (numericCode < 0n || numericCode > kMaxQuicErrorCode) {
      throw new ERR_OUT_OF_RANGE('options.errorCode',
                                 `>= 0 and <= ${kMaxQuicErrorCode}`,
                                 errorCode);
    }
    super(message);
    this.code = code;
    this.#errorCode = numericCode;
    this.#type = type;
  }

  /** @type {bigint} */
  get errorCode() {
    return this.#errorCode;
  }

  /** @type {'transport' | 'application'} */
  get type() {
    return this.#type;
  }
}

// Converts a raw QuicError array [type, code, reason] from C++ into a
// proper Node.js Error object.
function convertQuicError(error) {
  const type = error[0];
  const code = error[1];
  const reason = error[2];
  switch (type) {
    case 'transport':
      return new ERR_QUIC_TRANSPORT_ERROR(code, reason);
    case 'application':
      return new ERR_QUIC_APPLICATION_ERROR(code, reason);
    case 'version_negotiation':
      return new ERR_QUIC_VERSION_NEGOTIATION_ERROR();
    default:
      return new ERR_QUIC_TRANSPORT_ERROR(code, reason);
  }
}

// Convert a JavaScript error into close options suitable for
// `session.close()` / `session.destroy(error, options)`. The returned
// shape is `{ code, type, reason }` matching what `validateCloseOptions`
// expects (and what the native side reads via `MaybeSetCloseError`).
//
// Used so that destroying a session with an error actually emits a
// CONNECTION_CLOSE frame on the wire, instead of dropping the connection
// silently and leaving the peer waiting on its idle timer.
//
// Returns `undefined` when no error was supplied (caller falls back to
// a clean / silent close).
function errorToCloseOptions(error) {
  if (error === undefined || error === null) return undefined;
  // Generic mapping for now: any error becomes a transport-level
  // INTERNAL_ERROR (NGTCP2_INTERNAL_ERROR == 0x1) with the original
  // error message used as the human-readable reason. Future work could
  // detect specific `ERR_QUIC_*` subclasses and round-trip their
  // original code/type back onto the wire.
  const reason = typeof error === 'object' && error !== null && error.message ?
    `${error.message}` :
    `${error}`;
  return { code: 0x1n, type: 'transport', reason };
}

/**
 * Safely invoke a user-supplied callback. If the callback throws
 * synchronously, the owning object is destroyed with the error. If the
 * callback returns a promise that rejects, the rejection is caught and the
 * owning object is destroyed. Sync callbacks that do not throw incur no
 * promise allocation overhead.
 * @param {Function} fn  The callback to invoke.
 * @param {object} owner The QuicSession or QuicStream that owns the callback.
 * @param  {...any} args Arguments forwarded to the callback.
 */
function safeCallbackInvoke(fn, owner, ...args) {
  try {
    const result = fn(...args, owner);
    if (isPromise(result)) {
      // Block body - do NOT return the result of `owner.destroy(err)`.
      // For some owners (e.g. `QuicEndpoint`), `destroy(err)` returns the
      // owner's `closed` promise which itself eventually rejects with
      // the same error. If we let that propagate through the `.then()`
      // chain promise, nobody is awaiting that chain and we surface the
      // rejection as unhandled.
      PromisePrototypeThen(result, undefined, (err) => {
        owner.destroy(err);
      });
    }
  } catch (err) {
    owner.destroy(err);
  }
}

/**
 * Invoke an onerror callback. If the callback itself throws synchronously
 * or returns a promise that rejects, a SuppressedError wrapping both the
 * onerror failure and the original error is surfaced as an uncaught exception.
 * @param {Function} fn  The onerror callback.
 * @param {any} error    The original error that triggered destruction.
 */
function invokeOnerror(fn, error) {
  try {
    const result = fn(error);
    if (isPromise(result)) {
      PromisePrototypeThen(result, undefined, (err) => {
        process.nextTick(() => {
          // eslint-disable-next-line no-restricted-syntax
          throw new SuppressedError(err, error, err?.message);
        });
      });
    }
  } catch (err) {
    process.nextTick(() => {
      // eslint-disable-next-line no-restricted-syntax
      throw new SuppressedError(err, error, err?.message);
    });
  }
}

function validateBody(body) {
  if (body === undefined) return body;
  // ArrayBuffers, SharedArrayBuffers, and ArrayBufferViews are passed
  // through to the C++ layer which copies the bytes into its own
  // BackingStore. Callers can therefore safely reuse or mutate their
  // input buffers after the call returns. Callers that want to ensure
  // their buffer cannot be mutated after handing it off (for example,
  // when sharing the source with another async consumer) can call
  // ArrayBuffer.prototype.transfer() themselves before passing the
  // buffer.
  if (isArrayBuffer(body) ||
      isSharedArrayBuffer(body) ||
      isArrayBufferView(body)) {
    return body;
  }
  if (isBlob(body)) return body[kBlobHandle];

  // Strings are encoded as UTF-8.
  if (typeof body === 'string') {
    return Buffer.from(body, 'utf8');
  }

  // FileHandle -- lock it and pass the C++ handle to GetDataQueueFromSource
  // which creates an fd-backed DataQueue entry from the file path.
  if (body instanceof FileHandle) {
    if (body[kFileLocked]) {
      throw new ERR_INVALID_STATE('FileHandle is locked');
    }
    body[kFileLocked] = true;
    return body[kFileHandle];
  }

  throw new ERR_INVALID_ARG_TYPE('options.body', [
    'string',
    'ArrayBuffer',
    'ArrayBufferView',
    'Blob',
    'FileHandle',
  ], body);
}

/**
 * Parses an alternating [name, value, name, value, ...] array from C++
 * into a plain header object. Multi-value headers become arrays.
 * @param {string[]} pairs
 * @returns {object}
 */
function parseHeaderPairs(pairs) {
  assert(ArrayIsArray(pairs));
  assert(pairs.length % 2 === 0);
  const block = { __proto__: null };
  for (let n = 0; n + 1 < pairs.length; n += 2) {
    if (block[pairs[n]] !== undefined) {
      if (ArrayIsArray(block[pairs[n]])) {
        ArrayPrototypePush(block[pairs[n]], pairs[n + 1]);
      } else {
        block[pairs[n]] = [block[pairs[n]], pairs[n + 1]];
      }
    } else {
      block[pairs[n]] = pairs[n + 1];
    }
  }
  return block;
}

/**
 * Applies session and stream callbacks from an options object to a session.
 * @param {QuicSession} session
 * @param {object} cbs
 */
function applyCallbacks(session, cbs) {
  if (cbs.onerror) session.onerror = cbs.onerror;
  if (cbs.onstream) session.onstream = cbs.onstream;
  if (cbs.ondatagram) session.ondatagram = cbs.ondatagram;
  if (cbs.ondatagramstatus) session.ondatagramstatus = cbs.ondatagramstatus;
  if (cbs.onpathvalidation) session.onpathvalidation = cbs.onpathvalidation;
  if (cbs.onsessionticket) session.onsessionticket = cbs.onsessionticket;
  if (cbs.onversionnegotiation) session.onversionnegotiation = cbs.onversionnegotiation;
  if (cbs.onhandshake) session.onhandshake = cbs.onhandshake;
  if (cbs.onnewtoken) session.onnewtoken = cbs.onnewtoken;
  if (cbs.onearlyrejected) session.onearlyrejected = cbs.onearlyrejected;
  if (cbs.onorigin) session.onorigin = cbs.onorigin;
  if (cbs.ongoaway) session.ongoaway = cbs.ongoaway;
  if (cbs.onkeylog) session.onkeylog = cbs.onkeylog;
  if (cbs.onqlog) session.onqlog = cbs.onqlog;
  if (cbs.onheaders || cbs.ontrailers || cbs.oninfo || cbs.onwanttrailers) {
    session[kStreamCallbacks] = {
      __proto__: null,
      onheaders: cbs.onheaders,
      ontrailers: cbs.ontrailers,
      oninfo: cbs.oninfo,
      onwanttrailers: cbs.onwanttrailers,
    };
  }
}

/**
 * Configures the outbound data source for a stream. Detects the source
 * type and calls the appropriate C++ method.
 * @param {object} handle The C++ stream handle
 * @param {QuicStream} stream The JS stream object
 * @param {*} body The body source
 */
const kDefaultHighWaterMark = 65536;
const kDefaultMaxPendingDatagrams = 128;

function configureOutbound(handle, stream, body) {
  // body: null - close writable side immediately (FIN)
  if (body === null) {
    handle.initStreamingSource();
    handle.endWrite();
    return;
  }

  // Handle Promise - await and recurse. Native promises auto-flatten,
  // so the resolved value will never itself be a promise.
  if (isPromise(body)) {
    PromisePrototypeThen(
      body,
      (resolved) => configureOutbound(handle, stream, resolved),
      (err) => {
        if (!stream.destroyed) {
          stream.destroy(err);
        }
      },
    );
    return;
  }

  // Tier: One-shot - string (checked before sync iterable since
  // strings are iterable but we want the one-shot path).
  // Buffer.from may return a pooled buffer whose ArrayBuffer cannot
  // be transferred, so run it through validateBody which copies when
  // the buffer is a partial view of a larger ArrayBuffer.
  if (typeof body === 'string') {
    handle.attachSource(validateBody(Buffer.from(body, 'utf8')));
    return;
  }

  // Tier: One-shot - FileHandle. The C++ layer creates an fd-backed
  // DataQueue entry from the file path. The FileHandle is locked to
  // prevent concurrent use and closed automatically when the stream
  // finishes.
  if (body instanceof FileHandle) {
    if (body[kFileLocked]) {
      throw new ERR_INVALID_STATE('FileHandle is locked');
    }
    body[kFileLocked] = true;
    handle.attachSource(body[kFileHandle]);
    return;
  }

  // Tier: One-shot - ArrayBuffer, SharedArrayBuffer, TypedArray,
  // DataView, Blob. validateBody handles transfer-vs-copy logic,
  // SharedArrayBuffer copying, and partial view safety.
  if (isArrayBuffer(body) || isSharedArrayBuffer(body) ||
      isArrayBufferView(body) || isBlob(body)) {
    handle.attachSource(validateBody(body));
    return;
  }

  // Tier: Streaming - AsyncIterable (ReadableStream, stream.Readable,
  // async generators, etc.). Checked before sync iterable because some
  // objects implement both protocols and we prefer async.
  if (isAsyncIterable(body)) {
    consumeAsyncSource(handle, stream, body);
    return;
  }

  // Tier: Sync iterable - consumed synchronously
  if (isSyncIterable(body)) {
    consumeSyncSource(handle, stream, body);
    return;
  }

  throw new ERR_INVALID_ARG_TYPE(
    'body',
    ['string', 'ArrayBuffer', 'SharedArrayBuffer', 'TypedArray',
     'Blob', 'Iterable', 'AsyncIterable', 'Promise', 'null'],
    body,
  );
}

// Sets the high water mark and initial writeDesiredSize for a streaming
// outbound source. Called after handle.initStreamingSource() for both
// body-source and writer paths. One-shot body sources (string, Uint8Array,
// Blob, FileHandle, etc.) do not use this -- they go through attachSource
// and are not subject to backpressure.
function initStreamingBackpressure(stream) {
  const state = getQuicStreamState(stream);
  // Only set defaults if the user hasn't already configured them
  // (e.g., via createBidirectionalStream({ highWaterMark: N })).
  if (state.highWaterMark === 0) {
    state.highWaterMark = kDefaultHighWaterMark;
  }
  if (state.writeDesiredSize === 0) {
    state.writeDesiredSize = state.highWaterMark;
  }
}

// Waits for the stream's drain callback to fire, indicating the
// outbound has capacity for more data.
function waitForDrain(stream) {
  const { promise, resolve } = PromiseWithResolvers();
  const prevDrain = stream[kDrain];
  stream[kDrain] = () => {
    stream[kDrain] = prevDrain;
    resolve();
  };
  return promise;
}

// Writes a batch to the handle, awaiting drain if backpressured.
// Returns true if the stream was destroyed during the wait.
// Checks writeDesiredSize before writing to enforce backpressure
// against the outbound DataQueue's uncommitted bytes.
async function writeBatchWithDrain(handle, stream, batch) {
  const state = getQuicStreamState(stream);

  // Calculate total batch size for the capacity check.
  let len = 0;
  for (const chunk of batch) len += TypedArrayPrototypeGetByteLength(chunk);

  // If insufficient capacity, wait for the C++ drain signal which
  // fires when writeDesiredSize transitions from 0 to > 0 (i.e.,
  // ngtcp2 has consumed data from the outbound DataQueue).
  if (len > state.writeDesiredSize) {
    await waitForDrain(stream);
    if (stream.destroyed) return true;
  }

  // Write the batch. The return value is the total queued byte count
  // on success, or undefined on failure (e.g., DataQueue append
  // rejected). Guard against silent data loss.
  const result = handle.write(batch);
  if (result === undefined) {
    if (!stream.destroyed) {
      stream.destroy(new ERR_INVALID_STATE('Stream write failed'));
    }
    return true;
  }
  return false;
}

async function consumeAsyncSource(handle, stream, source) {
  handle.initStreamingSource();
  initStreamingBackpressure(stream);
  try {
    // Normalize to AsyncIterable<Uint8Array[]>
    const normalized = streamFrom(source);
    for await (const batch of normalized) {
      if (stream.destroyed) return;
      if (await writeBatchWithDrain(handle, stream, batch)) return;
    }
    handle.endWrite();
  } catch (err) {
    if (!stream.destroyed) {
      stream.destroy(err);
    } else {
      throw err;
    }
  }
}

async function consumeSyncSource(handle, stream, source) {
  handle.initStreamingSource();
  initStreamingBackpressure(stream);
  // Normalize to Iterable<Uint8Array[]>. Manually iterate so we can
  // pause between next() calls when backpressure hits.
  const normalized = streamFromSync(source);
  const iter = normalized[SymbolIterator]();
  try {
    while (true) {
      if (stream.destroyed) return;
      const { value: batch, done } = iter.next();
      if (done) break;
      if (await writeBatchWithDrain(handle, stream, batch)) return;
    }
    handle.endWrite();
  } catch (err) {
    if (!stream.destroyed) {
      stream.destroy(err);
    } else {
      // If the stream is already destroyed, rethrow the error to avoid
      // silently swallowing it. Tho in practice this shouldn't happen.
      throw err;
    }
  }
}

function isAsyncIterable(obj) {
  return obj != null && typeof obj[SymbolAsyncIterator] === 'function';
}

function isSyncIterable(obj) {
  return obj != null && typeof obj[SymbolIterator] === 'function';
}

// Functions used specifically for internal or assertion purposes only.
let getQuicStreamState;
let getQuicSessionState;
let getQuicEndpointState;
let assertIsQuicEndpoint;
let assertEndpointNotClosedOrClosing;
let assertEndpointIsNotBusy;

function maybeGetCloseError(context, status, pendingError) {
  switch (context) {
    case kCloseContextClose: {
      return pendingError;
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

class QuicStream {
  /** @type {object} */
  #handle;
  /**
   * Flag set at the top of `destroy()` to make the method safely
   * re-entrant. Distinct from `#handle === undefined` (which signals
   * "fully destroyed" and is set inside `[kFinishClose]`) so that
   * `[kFinishClose]`'s own destroyed-guard does not bail before the
   * cleanup work runs.
   * @type {boolean}
   */
  #destroying = false;
  /** @type {QuicSession} */
  #session;
  /** @type {QuicStreamStats} */
  #stats;
  /** @type {QuicStreamState} */
  #state;
  /** @type {number} */
  #direction = undefined;
  /** @type {Function|undefined} */
  #onerror = undefined;
  /** @type {OnBlockedCallback|undefined} */
  #onblocked = undefined;
  /** @type {OnStreamErrorCallback|undefined} */
  #onreset = undefined;
  /** @type {Function|undefined} */
  #onheaders = undefined;
  /** @type {Function|undefined} */
  #ontrailers = undefined;
  /** @type {Function|undefined} */
  #oninfo = undefined;
  /** @type {Function|undefined} */
  #onwanttrailers = undefined;
  /** @type {object|undefined} */
  #headers = undefined;
  /** @type {object|undefined} */
  #pendingTrailers = undefined;
  /** @type {Promise<void>} */
  #pendingClose = PromiseWithResolvers();
  #reader;
  #iteratorLocked = false;
  #writer = undefined;
  #outboundSet = false;
  /** @type {FileHandle|undefined} */
  #fileHandle = undefined;

  static {
    getQuicStreamState = function(stream) {
      QuicStream.#assertIsQuicStream(stream);
      return stream.#state;
    };
  }

  static #assertIsQuicStream(val) {
    if (val == null || !(#handle in val)) {
      throw new ERR_INVALID_THIS('QuicStream');
    }
  }

  #assertHeadersSupported() {
    if (getQuicSessionState(this.#session).headersSupported === 2) {
      throw new ERR_INVALID_STATE(
        'The negotiated QUIC application protocol does not support headers');
    }
  }

  /**
   * @param {symbol} privateSymbol
   * @param {object} handle
   * @param {QuicSession} session
   * @param {number} direction
   */
  constructor(privateSymbol, handle, session, direction) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }

    this.#handle = handle;
    this.#handle[kOwner] = this;
    this.#session = session;
    this.#direction = direction;
    this.#stats = new QuicStreamStats(kPrivateConstructor, this.#handle.stats);
    this.#state = new QuicStreamState(kPrivateConstructor, this.#handle.state);
    this.#reader = this.#handle.getReader();

    if (hasObserver('quic')) {
      startPerf(this, kPerfEntry, { type: 'quic', name: 'QuicStream' });
    }

    if (this.pending) {
      debug(`pending ${this.direction} stream created`);
    } else {
      debug(`${this.direction} stream ${this.id} created`);
    }
  }

  get [kValidatedSource]() { return true; }

  /**
   * Returns an AsyncIterator that yields Uint8Array[] batches of
   * incoming data. Only one iterator can be obtained per stream.
   * Non-readable streams return an immediately-finished iterator.
   * @yields {Uint8Array[]}
   */
  async *[SymbolAsyncIterator]() {
    QuicStream.#assertIsQuicStream(this);
    if (this.#iteratorLocked) {
      throw new ERR_INVALID_STATE('Stream is already being read');
    }
    this.#iteratorLocked = true;

    // Non-readable stream (outbound-only unidirectional, or closed)
    if (!this.#reader) return;

    yield* createBlobReaderIterable(this.#reader, {
      getReadError: () => {
        // The read side ends for one of three reasons:
        //   * Clean FIN received from the peer (state.finReceived
        //     === true). The iterator stops without calling this;
        //     fall through to the generic state error if it does.
        //   * Peer sent us a RESET_STREAM. The C++ side records the
        //     code in state.resetCode regardless of whether the JS
        //     onreset handler was attached. state.finReceived stays
        //     false because no FIN was seen.
        //   * We aborted locally via stream.resetStream() or
        //     stream.stopSending(). Both paths run EndReadable in
        //     C++, setting state.readEnded without setting
        //     state.finReceived. There is no peer code to surface.
        if (this.#state.readEnded && !this.#state.finReceived) {
          const peerResetCode = this.#state.resetCode;
          if (peerResetCode !== undefined && peerResetCode > 0n) {
            return new ERR_QUIC_STREAM_RESET(Number(peerResetCode));
          }
          return new ERR_QUIC_STREAM_ABORTED(
            'Stream aborted before FIN was received');
        }
        return new ERR_INVALID_STATE('The stream is not readable');
      },
    });
  }

  /**
   * True if the stream is still pending (i.e. it has not yet been opened
   * and assigned an ID).
   * @type {boolean}
   */
  get pending() {
    QuicStream.#assertIsQuicStream(this);
    return this.#state.pending;
  }

  /**
   * True if any data on this stream was received as 0-RTT (early data)
   * before the TLS handshake completed. Early data is less secure and
   * could be replayed by an attacker.
   * @type {boolean}
   */
  get early() {
    QuicStream.#assertIsQuicStream(this);
    return this.#state.early;
  }

  /**
   * The high water mark for write backpressure. When the total queued
   * outbound bytes exceeds this value, writeSync returns false and
   * desiredSize drops to 0. Default is 65536 (64KB).
   * @type {number}
   */
  get highWaterMark() {
    QuicStream.#assertIsQuicStream(this);
    return this.#state.highWaterMark;
  }

  set highWaterMark(val) {
    QuicStream.#assertIsQuicStream(this);
    validateInteger(val, 'highWaterMark', 0, 0xFFFFFFFF);
    this.#state.highWaterMark = val;
    // If writeDesiredSize hasn't been set yet (still 0 from initialization),
    // initialize it to the highWaterMark so the first write can proceed.
    if (this.#state.writeDesiredSize === 0 && val > 0) {
      this.#state.writeDesiredSize = val;
    }
  }

  /** @type {Function|undefined} */
  get onerror() {
    QuicStream.#assertIsQuicStream(this);
    return this.#onerror;
  }

  set onerror(fn) {
    QuicStream.#assertIsQuicStream(this);
    if (fn === undefined) {
      this.#onerror = undefined;
    } else {
      validateFunction(fn, 'onerror');
      this.#onerror = FunctionPrototypeBind(fn, this);
      markPromiseAsHandled(this.#pendingClose.promise);
    }
  }

  /** @type {OnBlockedCallback} */
  get onblocked() {
    QuicStream.#assertIsQuicStream(this);
    return this.#onblocked;
  }

  set onblocked(fn) {
    QuicStream.#assertIsQuicStream(this);
    if (fn === undefined) {
      this.#onblocked = undefined;
      this.#state.wantsBlock = false;
    } else {
      validateFunction(fn, 'onblocked');
      this.#onblocked = FunctionPrototypeBind(fn, this);
      this.#state.wantsBlock = true;
    }
  }

  /** @type {OnStreamErrorCallback} */
  get onreset() {
    QuicStream.#assertIsQuicStream(this);
    return this.#onreset;
  }

  set onreset(fn) {
    QuicStream.#assertIsQuicStream(this);
    if (fn === undefined) {
      this.#onreset = undefined;
      this.#state.wantsReset = false;
    } else {
      validateFunction(fn, 'onreset');
      this.#onreset = FunctionPrototypeBind(fn, this);
      this.#state.wantsReset = true;
    }
  }

  /** @type {OnHeadersCallback} */
  get onheaders() {
    QuicStream.#assertIsQuicStream(this);
    return this.#onheaders;
  }

  set onheaders(fn) {
    QuicStream.#assertIsQuicStream(this);
    if (fn === undefined) {
      this.#onheaders = undefined;
      this.#state[kWantsHeaders] = false;
    } else {
      this.#assertHeadersSupported();
      validateFunction(fn, 'onheaders');
      this.#onheaders = FunctionPrototypeBind(fn, this);
      this.#state[kWantsHeaders] = true;
    }
  }

  /** @type {Function|undefined} */
  get oninfo() {
    QuicStream.#assertIsQuicStream(this);
    return this.#oninfo;
  }

  set oninfo(fn) {
    QuicStream.#assertIsQuicStream(this);
    if (fn === undefined) {
      this.#oninfo = undefined;
    } else {
      this.#assertHeadersSupported();
      validateFunction(fn, 'oninfo');
      this.#oninfo = FunctionPrototypeBind(fn, this);
    }
  }

  /** @type {Function|undefined} */
  get ontrailers() {
    QuicStream.#assertIsQuicStream(this);
    return this.#ontrailers;
  }

  set ontrailers(fn) {
    QuicStream.#assertIsQuicStream(this);
    if (fn === undefined) {
      this.#ontrailers = undefined;
    } else {
      this.#assertHeadersSupported();
      validateFunction(fn, 'ontrailers');
      this.#ontrailers = FunctionPrototypeBind(fn, this);
    }
  }

  /** @type {Function|undefined} */
  get onwanttrailers() {
    QuicStream.#assertIsQuicStream(this);
    return this.#onwanttrailers;
  }

  set onwanttrailers(fn) {
    QuicStream.#assertIsQuicStream(this);
    if (fn === undefined) {
      this.#onwanttrailers = undefined;
      this.#state[kWantsTrailers] = false;
    } else {
      this.#assertHeadersSupported();
      validateFunction(fn, 'onwanttrailers');
      this.#onwanttrailers = FunctionPrototypeBind(fn, this);
      this.#state[kWantsTrailers] = true;
    }
  }

  /**
   * The buffered initial headers received on this stream, or undefined
   * if the application does not support headers or no headers have
   * been received yet.
   * @type {object|undefined}
   */
  get headers() {
    QuicStream.#assertIsQuicStream(this);
    return this.#headers;
  }

  /**
   * Set trailing headers to be sent when nghttp3 asks for them.
   * @type {object|undefined}
   */
  get pendingTrailers() {
    QuicStream.#assertIsQuicStream(this);
    return this.#pendingTrailers;
  }

  set pendingTrailers(headers) {
    QuicStream.#assertIsQuicStream(this);
    if (headers === undefined) {
      this.#pendingTrailers = undefined;
      return;
    }
    if (getQuicSessionState(this.#session).headersSupported === 2) {
      throw new ERR_INVALID_STATE(
        'The negotiated QUIC application protocol does not support headers');
    }
    validateObject(headers, 'headers');
    this.#pendingTrailers = headers;
  }

  /**
   * The statistics collected for this stream.
   * @type {QuicStreamStats}
   */
  get stats() {
    QuicStream.#assertIsQuicStream(this);
    return this.#stats;
  }

  /**
   * The session this stream belongs to. If the stream is destroyed,
   * `null` will be returned.
   * @type {QuicSession | null}
   */
  get session() {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed) return null;
    return this.#session;
  }

  /**
   * Returns the id for this stream. If the stream is destroyed or still pending,
   * `null` will be returned.
   * @type {bigint | null}
   */
  get id() {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed || this.pending) return null;
    return this.#state.id;
  }

  /**
   * Returns the directionality of this stream. If the stream is destroyed
   * or still pending, `null` will be returned.
   * @type {'bidi'|'uni'|null}
   */
  get direction() {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed || this.pending) return null;
    return this.#direction === kStreamDirectionBidirectional ? 'bidi' : 'uni';
  }

  /**
   * True if the stream has been destroyed.
   * @type {boolean}
   */
  get destroyed() {
    QuicStream.#assertIsQuicStream(this);
    return this.#handle === undefined;
  }

  /**
   * A promise that will be resolved when the stream is closed.
   * @type {Promise<void>}
   */
  get closed() {
    QuicStream.#assertIsQuicStream(this);
    return this.#pendingClose.promise;
  }

  /**
   * Immediately destroys the stream. Any queued data is discarded. If
   * an error is given, the closed promise will be rejected with that
   * error. If no error is given, the closed promise will be resolved.
   * When destroying with an error, RESET_STREAM and/or STOP_SENDING
   * are emitted to the peer for any still-open writable / readable
   * side of the stream. The wire code is resolved as:
   * `options.code` -> `error.errorCode` (when `error` is a
   * `QuicError`) -> the negotiated application's "internal error"
   * code from `QuicSessionState.internalErrorCode`.
   * @param {any} error
   * @param {object} [options]
   * @param {bigint|number} [options.code] An explicit application
   *   error code to send on the resulting `RESET_STREAM` /
   *   `STOP_SENDING` frames. Numbers are coerced to `BigInt`. When
   *   omitted, the code is derived from `error` per the precedence
   *   above.
   * @param {string} [options.reason] Optional human-readable reason.
   *   Accepted for symmetry with `session.close()` /
   *   `session.destroy()`; QUIC `RESET_STREAM` and `STOP_SENDING`
   *   frames do not themselves carry a reason field over the wire.
   */
  destroy(error, options = kEmptyObject) {
    QuicStream.#assertIsQuicStream(this);
    // Two distinct guards:
    //   * `#destroying` flips synchronously here so any re-entrant call
    //     from inside this method's user callbacks hits the guard and
    //     returns immediately.
    //   * `destroyed` (i.e. `#handle === undefined`) catches the case
    //     where the C++ side already finished cleanup via the
    //     `onStreamClose -> [kFinishClose]` path - which does NOT go
    //     through `destroy()` and therefore never sets `#destroying`.
    //     `[kFinishClose]` clears `#handle` at the end of its work.
    if (this.#destroying || this.destroyed) return;
    // Validate options up front so a malformed `options` argument
    // throws before any side effects (mutating `#destroying`,
    // emitting wire frames, invoking `onerror`, settling the closed
    // promise). The caller may retry with valid options.
    validateObject(options, 'options');
    const { code: optionCode, reason } = options;
    if (optionCode !== undefined &&
        typeof optionCode !== 'bigint' &&
        typeof optionCode !== 'number') {
      throw new ERR_INVALID_ARG_TYPE('options.code',
                                     ['bigint', 'number'], optionCode);
    }
    if (reason !== undefined) {
      validateString(reason, 'options.reason');
    }
    this.#destroying = true;
    // Resolve the wire error code for any RESET_STREAM / STOP_SENDING
    // frames emitted below.
    let abortCode;
    if (optionCode !== undefined) {
      abortCode = BigInt(optionCode);
    } else if (error !== undefined) {
      abortCode = error instanceof QuicError ?
        error.errorCode :
        getQuicSessionState(this.#session).internalErrorCode;
    }
    // When destroying with an error, ensure the peer stops sending
    // data we are about to discard by emitting STOP_SENDING. The
    // condition gates the emission to error-path destroys with a
    // still-open readable side. Direction model for the readable
    // side:
    //   * bidi: always has a readable side.
    //   * uni + #reader !== undefined: remote-initiated, read-only.
    //   * uni + #reader === undefined: locally-initiated, write-only;
    //     no readable side to stop.
    if (abortCode !== undefined &&
        !this.#state.readEnded &&
        (this.#direction === kStreamDirectionBidirectional ||
         this.#reader !== undefined)) {
      this.#handle.stopSending(abortCode);
    }
    // When destroying with an error, ensure the peer learns about
    // it via RESET_STREAM. The writer.fail path inside [kFinishClose]
    // emits RESET_STREAM only when a writer has been created;
    // streams that destroy without ever accessing stream.writer
    // (e.g. used setBody or never wrote at all) need an explicit
    // RESET_STREAM here so the write side does not dangle on the
    // wire. The condition gates the emission to error-path destroys
    // with a still-open writable side.
    // Direction model for the writable side:
    //   * bidi: always has a writable side.
    //   * uni + #reader === undefined: locally-initiated, write-only.
    //   * uni + #reader !== undefined: remote-initiated, read-only;
    //     no writable side to reset.
    if (abortCode !== undefined &&
        this.#writer === undefined &&
        !this.#state.writeEnded &&
        (this.#direction === kStreamDirectionBidirectional ||
         this.#reader === undefined)) {
      this.#handle.resetStream(abortCode);
    }
    if (error !== undefined && typeof this.#onerror === 'function') {
      invokeOnerror(this.#onerror, error);
    }
    const handle = this.#handle;
    this[kFinishClose](error);
    handle.destroy();
  }

  /**
   * Sets the outbound data source for the stream. This can only be called
   * once and must be called before any data will be sent. The body can be
   * an ArrayBuffer, a TypedArray or DataView, or a Blob. If the stream
   * is destroyed or already has an outbound data source, an error will
   * be thrown.
   * @param {ArrayBuffer|SharedArrayBuffer|ArrayBufferView|Blob} outbound
   */
  setOutbound(outbound) {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed) {
      throw new ERR_INVALID_STATE('Stream is destroyed');
    }
    if (this.#state.hasOutbound) {
      throw new ERR_INVALID_STATE('Stream already has an outbound data source');
    }
    this.#handle.attachSource(validateBody(outbound));
  }

  /**
   * Send initial or response headers on this stream. Throws if the
   * application does not support headers.
   * @param {object} headers
   * @param {{ terminal?: boolean }} [options]
   * @returns {boolean}
   */
  sendHeaders(headers, options = kEmptyObject) {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed) return false;
    if (getQuicSessionState(this.#session).headersSupported === 2) {
      throw new ERR_INVALID_STATE(
        'The negotiated QUIC application protocol does not support headers');
    }
    validateObject(headers, 'headers');
    const { terminal = false } = options;
    const headerString = buildNgHeaderString(
      headers, assertValidPseudoHeader, true);
    const flags = terminal ? kHeadersFlagsTerminal : kHeadersFlagsNone;
    return this.#handle.sendHeaders(kHeadersKindInitial, headerString, flags);
  }

  /**
   * Send informational (1xx) headers on this stream. Server only.
   * Throws if the application does not support headers.
   * @param {object} headers
   * @returns {boolean}
   */
  sendInformationalHeaders(headers) {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed) return false;
    if (getQuicSessionState(this.#session).headersSupported === 2) {
      throw new ERR_INVALID_STATE(
        'The negotiated QUIC application protocol does not support headers');
    }
    validateObject(headers, 'headers');
    const headerString = buildNgHeaderString(
      headers, assertValidPseudoHeader, true);
    return this.#handle.sendHeaders(
      kHeadersKindHints, headerString, kHeadersFlagsNone);
  }

  /**
   * Send trailing headers on this stream. Must be called synchronously
   * during the onwanttrailers callback, or set via pendingTrailers before
   * the body completes. Throws if the application does not support headers.
   * @param {object} headers
   * @returns {boolean}
   */
  sendTrailers(headers) {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed) return false;
    if (getQuicSessionState(this.#session).headersSupported === 2) {
      throw new ERR_INVALID_STATE(
        'The negotiated QUIC application protocol does not support headers');
    }
    validateObject(headers, 'headers');
    const headerString = buildNgHeaderString(headers);
    return this.#handle.sendHeaders(
      kHeadersKindTrailing, headerString, kHeadersFlagsNone);
  }

  /**
   * Returns a Writer for pushing data to this stream incrementally.
   * Only available when no body source was provided at creation time
   * or via setBody(). Non-writable streams return an already-closed Writer.
   * @type {object}
   */
  get writer() {
    QuicStream.#assertIsQuicStream(this);
    if (this.#writer !== undefined) return this.#writer;
    if (this.#outboundSet) {
      throw new ERR_INVALID_STATE(
        'Stream outbound already configured with a body source');
    }

    const handle = this.#handle;
    const stream = this;
    let closed = false;
    let errored = false;
    let error = null;
    let totalBytesWritten = 0;
    let drainWakeup = null;

    // Drain callback - C++ fires this when send buffer has space
    stream[kDrain] = () => {
      if (drainWakeup) {
        drainWakeup.resolve(true);
        drainWakeup = null;
      }
    };

    // A note on backpressure handling: per the stream/iter spec, the default
    // backpressure policy for writers is strict, meaning that if the stream
    // signals backpressure additional writes are rejected until the buffer has
    // capacity again.

    function writeSync(chunk) {
      // If the stream is closed, errored, or write-ended, we cannot accept
      // more data. Refuse the sync write.
      // If a drain is already pending, another operation is waiting
      // for capacity. Refuse the sync write.
      if (closed || errored || stream.#state.writeEnded || drainWakeup != null) {
        return false;
      }
      chunk = toUint8Array(chunk);
      const len = TypedArrayPrototypeGetByteLength(chunk);
      if (len === 0) return true;
      // Refuse the write if the chunk doesn't fit in the available
      // buffer capacity. The caller should wait for drain and retry.
      if (len > stream.#state.writeDesiredSize) return false;
      const result = handle.write([chunk]);
      if (result === undefined) return false;
      totalBytesWritten += len;
      return true;
    }

    async function write(chunk, options = kEmptyObject) {
      validateObject(options, 'options');
      const { signal } = options;
      if (signal !== undefined) {
        validateAbortSignal(signal, 'options.signal');
        signal.throwIfAborted();
      }
      if (errored) throw error;
      if (closed || stream.#state.writeEnded) {
        throw new ERR_INVALID_STATE('Writer is closed');
      }
      // If a drain is already pending, another operation is waiting
      // for capacity. Under strict policy, reject immediately.
      // Later, if we add support for other backpressure policies,
      // we could instead await the existing drain before proceeding.
      if (drainWakeup != null) {
        throw new ERR_INVALID_STATE('Stream write buffer is full');
      }

      if (!writeSync(chunk)) {
        throw new ERR_INVALID_STATE('Stream write buffer is full');
      }
    }

    function writevSync(chunks) {
      if (closed || errored || stream.#state.writeEnded || drainWakeup != null) {
        return false;
      }
      chunks = convertChunks(chunks);
      let len = 0;
      for (const c of chunks) len += TypedArrayPrototypeGetByteLength(c);
      if (len === 0) return true;
      if (len > stream.#state.writeDesiredSize) return false;
      const result = handle.write(chunks);
      if (result === undefined) return false;
      totalBytesWritten += len;
      return true;
    }

    async function writev(chunks, options = kEmptyObject) {
      validateObject(options, 'options');
      const { signal } = options;
      if (signal !== undefined) {
        validateAbortSignal(signal, 'options.signal');
        signal.throwIfAborted();
      }

      if (errored) throw error;
      if (closed || stream.#state.writeEnded) {
        throw new ERR_INVALID_STATE('Writer is closed');
      }

      // If a drain is already pending, another operation is waiting
      // for capacity. Under strict policy, reject immediately.
      // Later, if we add support for other backpressure policies,
      // we could instead await the existing drain before proceeding.
      if (drainWakeup != null) {
        throw new ERR_INVALID_STATE('Stream write buffer is full');
      }

      if (!writevSync(chunks)) {
        throw new ERR_INVALID_STATE('Stream write buffer is full');
      }
    }

    function endSync() {
      // Per the streams/iter spec, endSync and end follow a try-fallback
      // pattern. That is, callers should try endSync first and if it returns
      // -1, then they should call and await end(). This is a signal that sync
      // end is not currently possible. However, we always support sync end
      // here unless the stream is already errored.
      if (errored) return -1;

      // If we're already closed, just return the total bytes written.
      if (closed) return totalBytesWritten;

      // If we are waiting for drain to complete, we cannot end synchronously.
      if (drainWakeup != null) return -1;

      // Fantastic, we can end synchronously!
      handle.endWrite();
      closed = true;
      return totalBytesWritten;
    }

    async function end(options = kEmptyObject) {
      validateObject(options, 'options');
      const { signal } = options;
      if (signal !== undefined) {
        validateAbortSignal(signal, 'options.signal');
        signal.throwIfAborted();
        // TODO(@jasnell): The stream/iter spec allows individual sync end
        // calls to be canceled via an AbortSignal. We currently do not support
        // this, but we can add before the impl is graduated from experimental.
        // At most we do here is check for signal abort at the start of the call.
      }

      // Per the streams/iter spec, endSync and end follow a try-fallback
      // pattern. That is, callers should try endSync first and if it returns
      // -1, then they should call and await end(). This is a signal that sync
      // end is not currently possible. However, we always support sync end
      // here unless the stream is already errored.
      // While the user should have already called endSync, we call it again
      // here to actually process the end request. At worst it's called twice.
      const n = endSync();

      // A return value of -1 indicates that endSync was not yet able to
      // process the end request, either because we are errored or because we
      // are awaiting drain. If we're errored, throw the error. If we're waiting
      // for drain, await it and then try ending again.

      if (n >= 0) return n;
      if (errored) throw error;

      drainWakeup ??= PromiseWithResolvers();
      try {
        await drainWakeup.promise;
      } finally {
        drainWakeup = null;
      }
      return endSync();
    }

    function fail(reason) {
      if (closed || errored) return;
      errored = true;
      error = reason ?? new ERR_INVALID_STATE('Failed');
      // `writer.fail()` is always an error path, so the wire code on
      // RESET_STREAM must never be `0n` (which means "no error" in
      // most application protocols). Resolve the code in priority
      // order:
      //   1. If `reason` is a `QuicError`, use its explicit
      //      `errorCode`.
      //   2. Otherwise fall back to the negotiated application's
      //      "internal error" code, surfaced via
      //      `QuicSessionState.internalErrorCode`. For HTTP/3 this is
      //      `H3_INTERNAL_ERROR` (0x102); for raw QUIC applications
      //      it falls back to the QUIC transport-layer
      //      `INTERNAL_ERROR` (0x1).
      const code = error instanceof QuicError ?
        error.errorCode :
        getQuicSessionState(stream.#session).internalErrorCode;
      handle.resetStream(code);
      if (drainWakeup != null) {
        drainWakeup.reject(error);
        drainWakeup = null;
      }
    }

    const writer = {
      __proto__: null,
      get desiredSize() {
        if (closed || errored || stream.#state.writeEnded) return null;
        return stream.#state.writeDesiredSize;
      },
      writeSync,
      write,
      writevSync,
      writev,
      endSync,
      end,
      fail,
      [drainableProtocol]() {
        if (closed || errored) return null;
        // If a drain is already pending, return the existing promise.
        if (drainWakeup != null) return drainWakeup.promise;
        if (stream.#state.writeDesiredSize > 0) return null;
        drainWakeup = PromiseWithResolvers();
        return drainWakeup.promise;
      },
      [SymbolAsyncDispose]() {
        if (!closed && !errored) fail();
        return PromiseResolve();
      },
      [SymbolDispose]() {
        if (!closed && !errored) fail();
      },
    };

    // Non-writable stream - return a pre-closed writer.
    // A readable unidirectional stream is a remote uni (read-only).
    if (!handle || this.destroyed || this.#state.writeEnded ||
        (this.#direction === kStreamDirectionUnidirectional &&
         this.#reader !== undefined)) {
      closed = true;
      this.#writer = writer;
      return this.#writer;
    }

    // Initialize the outbound DataQueue for streaming writes
    handle.initStreamingSource();
    initStreamingBackpressure(this);

    this.#writer = writer;
    return this.#writer;
  }

  /**
   * Sets the outbound body source for this stream. Accepts all body
   * source types (string, TypedArray, Blob, AsyncIterable, Promise, null).
   * Can only be called once. Mutually exclusive with stream.writer.
   * @param {*} body
   */
  setBody(body) {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed) {
      throw new ERR_INVALID_STATE('Stream is destroyed');
    }
    if (this.#outboundSet) {
      throw new ERR_INVALID_STATE('Stream outbound already configured');
    }
    if (this.#writer !== undefined) {
      throw new ERR_INVALID_STATE('Stream writer already accessed');
    }
    this.#outboundSet = true;
    // If the body is a FileHandle, store it so it is closed
    // automatically when the stream finishes.
    if (body instanceof FileHandle) {
      this.#fileHandle = body;
    }
    configureOutbound(this.#handle, this, body);
  }

  /**
   * Associates a FileHandle with this stream so it is closed automatically
   * when the stream finishes. Called internally when a FileHandle is used
   * as a body source.
   * @param {FileHandle} fh
   */
  [kAttachFileHandle](fh) {
    this.#fileHandle = fh;
  }

  /**
   * Tells the peer to stop sending data for this stream. The optional error
   * code will be sent to the peer as part of the request. If the stream is
   * already destroyed, this is a no-op. No acknowledgement of this action
   * will be provided.
   * @param {number|bigint} code
   */
  stopSending(code = 0n) {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed) return;
    this.#handle.stopSending(BigInt(code));
  }

  /**
   * Tells the peer that this end will not send any more data on this stream.
   * The optional error code will be sent to the peer as part of the
   * request. If the stream is already destroyed, this is a no-op. No
   * acknowledgement of this action will be provided.
   * @param {number|bigint} code
   */
  resetStream(code = 0n) {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed) return;
    this.#handle.resetStream(BigInt(code));
  }

  /**
   * The priority of the stream. If the stream is destroyed or if
   * the session does not support priority, `null` will be
   * returned.
   * @type {{ level: 'default' | 'low' | 'high', incremental: boolean } | null}
   */
  get priority() {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed ||
        !getQuicSessionState(this.#session).isPrioritySupported) return null;
    const packed = this.#handle.getPriority();
    const urgency = packed >> 1;
    const incremental = !!(packed & 1);
    const level = urgency < 3 ? 'high' : urgency > 3 ? 'low' : 'default';
    return { level, incremental };
  }

  /**
   * Sets the priority of the stream.
   * @param {{
   *   level?: 'default' | 'low' | 'high',
   *   incremental?: boolean,
   * }} options
   */
  setPriority(options = kEmptyObject) {
    QuicStream.#assertIsQuicStream(this);
    if (this.destroyed) return;
    if (!getQuicSessionState(this.#session).isPrioritySupported) {
      throw new ERR_INVALID_STATE(
        'The session does not support stream priority');
    }
    validateObject(options, 'options');
    const {
      level = 'default',
      incremental = false,
    } = options;
    validateOneOf(level, 'options.level', ['default', 'low', 'high']);
    validateBoolean(incremental, 'options.incremental');
    const urgency = level === 'high' ? 0 : level === 'low' ? 7 : 3;
    this.#handle.setPriority((urgency << 1) | (incremental ? 1 : 0));
  }

  /**
   * Send a block of headers. The headers are formatted as an array
   * of key, value pairs. The reason we don't use a Headers object
   * here is because this needs to be able to represent headers like
   * :method which the high-level Headers API does not allow.
   *
   * Note that QUIC in general does not support headers. This method
   * is in place to support HTTP3 and is therefore not generally
   * exposed except via a private symbol.
   * @param {object} headers
   * @returns {boolean} true if the headers were scheduled to be sent.
   */
  [kSendHeaders](headers, kind = kHeadersKindInitial,
                 flags = kHeadersFlagsTerminal) {
    validateObject(headers, 'headers');
    if (getQuicSessionState(this.#session).headersSupported === 2) {
      throw new ERR_INVALID_STATE(
        'The negotiated QUIC application protocol does not support headers');
    }
    if (this.pending) {
      debug('pending stream enqueuing headers', headers);
    } else {
      debug(`stream ${this.id} sending headers`, headers);
    }
    const headerString = buildNgHeaderString(
      headers,
      assertValidPseudoHeader,
      true, // This could become an option in future
    );
    return this.#handle.sendHeaders(kind, headerString, flags);
  }

  [kFinishClose](error) {
    if (this.destroyed) return this.#pendingClose.promise;
    if (error !== undefined) {
      if (this.pending) {
        debug(`destroying pending stream with error: ${error}`);
      } else {
        debug(`destroying stream ${this.id} with error: ${error}`);
      }
      this.#pendingClose.reject(error);
    } else {
      if (this.pending) {
        debug('destroying pending stream with no error');
      } else {
        debug(`destroying stream ${this.id} with no error`);
      }
      this.#pendingClose.resolve();
    }
    if (onStreamClosedChannel.hasSubscribers) {
      onStreamClosedChannel.publish({
        __proto__: null,
        stream: this,
        session: this.#session,
        error,
        stats: this.stats,
      });
    }
    if (this[kPerfEntry] && hasObserver('quic')) {
      stopPerf(this, kPerfEntry, {
        detail: {
          stats: this.stats,
          direction: this.direction,
        },
      });
    }
    this.#stats[kFinishClose]();
    this.#state[kFinishClose]();
    this.#session[kRemoveStream](this);
    if (this.#writer !== undefined) {
      this.#writer.fail(error);
    }
    this.#session = undefined;
    this.#pendingClose.reject = undefined;
    this.#pendingClose.resolve = undefined;
    this.#onblocked = undefined;
    this.#onreset = undefined;
    this.#onheaders = undefined;
    this.#onerror = undefined;
    this.#ontrailers = undefined;
    this.#oninfo = undefined;
    this.#onwanttrailers = undefined;
    this.#headers = undefined;
    this.#pendingTrailers = undefined;
    this.#handle = undefined;
    if (this.#fileHandle !== undefined) {
      // Close the FileHandle that was used as a body source. The close
      // may fail if the user already closed it -- that's expected and
      // harmless, so mark the promise as handled.
      markPromiseAsHandled(this.#fileHandle.close());
      this.#fileHandle = undefined;
    }
  }

  [kBlocked]() {
    // The blocked event should only be called if the stream was created with
    // an onblocked callback. The callback should always exist here.
    assert(this.#onblocked, 'Unexpected stream blocked event');
    if (onStreamBlockedChannel.hasSubscribers) {
      onStreamBlockedChannel.publish({
        __proto__: null,
        stream: this,
        session: this.#session,
      });
    }
    safeCallbackInvoke(this.#onblocked, this);
  }

  [kDrain]() {
    // No-op by default. Overridden by the writer closure when
    // stream.writer is accessed.
  }

  [kReset](error) {
    // The reset event should only be called if the stream was created with
    // an onreset callback. The callback should always exist here.
    assert(this.#onreset, 'Unexpected stream reset event');
    if (onStreamResetChannel.hasSubscribers) {
      onStreamResetChannel.publish({
        __proto__: null,
        stream: this,
        session: this.#session,
        error,
      });
    }
    safeCallbackInvoke(this.#onreset, this, error);
  }

  [kHeaders](headers, kind) {
    const block = parseHeaderPairs(headers);
    const kindName = kHeadersKindName[kind] ?? kind;

    switch (kindName) {
      case 'initial':
        assert(this.#onheaders, 'Unexpected stream headers event');
        if (this.#headers === undefined) this.#headers = block;
        if (onStreamHeadersChannel.hasSubscribers) {
          onStreamHeadersChannel.publish({
            __proto__: null,
            stream: this,
            session: this.#session,
            headers: block,
          });
        }
        safeCallbackInvoke(this.#onheaders, this, block);
        break;
      case 'trailing':
        if (onStreamTrailersChannel.hasSubscribers) {
          onStreamTrailersChannel.publish({
            __proto__: null,
            stream: this,
            session: this.#session,
            trailers: block,
          });
        }
        if (this.#ontrailers)
          safeCallbackInvoke(this.#ontrailers, this, block);
        break;
      case 'hints':
        if (onStreamInfoChannel.hasSubscribers) {
          onStreamInfoChannel.publish({
            __proto__: null,
            stream: this,
            session: this.#session,
            headers: block,
          });
        }
        if (this.#oninfo)
          safeCallbackInvoke(this.#oninfo, this, block);
        break;
    }
  }

  [kTrailers]() {
    if (this.destroyed) return;

    // nghttp3 is asking us to provide trailers to send.
    // Check for pre-set pendingTrailers first, then the callback.
    if (this.#pendingTrailers) {
      this.sendTrailers(this.#pendingTrailers);
      this.#pendingTrailers = undefined;
    } else if (this.#onwanttrailers) {
      safeCallbackInvoke(this.#onwanttrailers, this);
    }
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `QuicStream ${inspect({
      __proto__: null,
      id: this.id,
      direction: this.direction,
      pending: this.pending,
      stats: this.stats,
      state: this.#state,
      session: this.session,
    }, opts)}`;
  }
}

class QuicSession {
  /** @type {QuicEndpoint} */
  #endpoint = undefined;
  /** @type {boolean} */
  #isPendingClose = false;
  /** @type {boolean} */
  #selfInitiatedClose = false;
  /**
   * Flag set at the top of `destroy()` to make the method safely
   * re-entrant. Distinct from `#handle === undefined` so callbacks
   * that fire from C++ during teardown (e.g. `onSessionClose` ->
   * `[kFinishClose]`) still see a live `#handle` and can complete
   * their work.
   * @type {boolean}
   */
  #destroying = false;
  /**
   * Set to `true` once the TLS handshake has completed successfully
   * (i.e. `[kHandshake]` has fired). Used to gate operations that only
   * make sense for a fully-opened session - notably, attempting to
   * send a `CONNECTION_CLOSE` from `endpoint.destroy(error)` cascade.
   * The C++ side cannot create a valid `CONNECTION_CLOSE` packet
   * before handshake completion and falls back to a path that
   * re-enters JS `destroy()` and trips our `#destroying` guard,
   * leaving the C++ side asserting an inconsistent state.
   * @type {boolean}
   */
  #handshakeCompleted = false;
  /** @type {object|undefined} */
  #handle;
  /** @type {PromiseWithResolvers<void>} */
  #pendingClose = PromiseWithResolvers();
  /** @type {PromiseWithResolvers<QuicSessionInfo>} */
  #pendingOpen = PromiseWithResolvers();
  /** @type {QuicSessionState} */
  #state;
  /** @type {QuicSessionStats} */
  #stats;
  /** @type {Set<QuicStream>} */
  #streams = new SafeSet();
  /** @type {Function|undefined} */
  #onerror = undefined;
  /** @type {OnStreamCallback} */
  #onstream = undefined;
  /** @type {OnDatagramCallback|undefined} */
  #ondatagram = undefined;
  /** @type {OnDatagramStatusCallback|undefined} */
  #ondatagramstatus = undefined;
  /** @type {Function|undefined} */
  #onpathvalidation = undefined;
  /** @type {Function|undefined} */
  #onsessionticket = undefined;
  /** @type {Function|undefined} */
  #onversionnegotiation = undefined;
  /** @type {Function|undefined} */
  #onhandshake = undefined;
  /** @type {Function|undefined} */
  #onnewtoken = undefined;
  /** @type {Function|undefined} */
  #onearlyrejected = undefined;
  /** @type {Function|undefined} */
  #onorigin = undefined;
  /** @type {Function|undefined} */
  #ongoaway = undefined;
  /** @type {Function|undefined} */
  #onkeylog = undefined;
  /** @type {Function|undefined} */
  #onqlog = undefined;
  #pendingQlog = undefined;
  #handshakeInfo = undefined;
  /** @type {{ local: SocketAddress, remote: SocketAddress }|undefined} */
  #path = undefined;
  #certificate = undefined;
  #peerCertificate = undefined;
  #ephemeralKeyInfo = undefined;

  static {
    getQuicSessionState = function(session) {
      QuicSession.#assertIsQuicSession(session);
      return session.#state;
    };
  }

  static #assertIsQuicSession(val) {
    if (val == null || !(#handle in val)) {
      throw new ERR_INVALID_THIS('QuicSession');
    }
  }

  /**
   * @param {symbol} privateSymbol
   * @param {object} handle
   * @param {QuicEndpoint} endpoint
   */
  constructor(privateSymbol, handle, endpoint) {
    // Instances of QuicSession can only be created internally.
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }

    this.#endpoint = endpoint;
    this.#handle = handle;
    this.#handle[kOwner] = this;
    // Move any qlog entries that arrived before the wrapper existed.
    if (handle._pendingQlog !== undefined) {
      this.#pendingQlog = handle._pendingQlog;
      handle._pendingQlog = undefined;
    }
    this.#stats = new QuicSessionStats(kPrivateConstructor, handle.stats);
    this.#state = new QuicSessionState(kPrivateConstructor, handle.state);

    if (hasObserver('quic')) {
      startPerf(this, kPerfEntry, { type: 'quic', name: 'QuicSession' });
    }

    debug('session created');
  }

  /** @type {boolean} */
  get #isClosedOrClosing() {
    return this.#handle === undefined || this.#isPendingClose;
  }

  /** @type {Function|undefined} */
  get onerror() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onerror;
  }

  set onerror(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onerror = undefined;
    } else {
      validateFunction(fn, 'onerror');
      this.#onerror = FunctionPrototypeBind(fn, this);
      // When an onerror handler is provided, mark the pending promises
      // as handled so that rejections from destroy(error) don't surface
      // as unhandled rejections. The onerror callback is the
      // application's error handler for this session.
      markPromiseAsHandled(this.#pendingClose.promise);
      markPromiseAsHandled(this.#pendingOpen.promise);
      // Also mark existing streams' closed promises. Stream rejections
      // during session destruction are expected collateral when the
      // session has an error handler.
      for (const stream of this.#streams) {
        markPromiseAsHandled(stream.closed);
      }
    }
  }

  /** @type {OnStreamCallback} */
  get onstream() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onstream;
  }

  set onstream(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onstream = undefined;
    } else {
      validateFunction(fn, 'onstream');
      this.#onstream = FunctionPrototypeBind(fn, this);
    }
  }

  /** @type {OnDatagramCallback} */
  get ondatagram() {
    QuicSession.#assertIsQuicSession(this);
    return this.#ondatagram;
  }

  set ondatagram(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#ondatagram = undefined;
      this.#state.hasDatagramListener = false;
    } else {
      validateFunction(fn, 'ondatagram');
      this.#ondatagram = FunctionPrototypeBind(fn, this);
      this.#state.hasDatagramListener = true;
    }
  }

  /**
   * The ondatagramstatus callback is called when the status of a sent datagram
   * is received. This is best-effort only.
   * @type {OnDatagramStatusCallback}
   */
  get ondatagramstatus() {
    QuicSession.#assertIsQuicSession(this);
    return this.#ondatagramstatus;
  }

  set ondatagramstatus(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#ondatagramstatus = undefined;
      this.#state.hasDatagramStatusListener = false;
    } else {
      validateFunction(fn, 'ondatagramstatus');
      this.#ondatagramstatus = FunctionPrototypeBind(fn, this);
      this.#state.hasDatagramStatusListener = true;
    }
  }

  /** @type {Function|undefined} */
  get onpathvalidation() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onpathvalidation;
  }

  set onpathvalidation(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onpathvalidation = undefined;
      this.#state.hasPathValidationListener = false;
    } else {
      validateFunction(fn, 'onpathvalidation');
      this.#onpathvalidation = FunctionPrototypeBind(fn, this);
      this.#state.hasPathValidationListener = true;
    }
  }

  get onkeylog() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onkeylog;
  }

  set onkeylog(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onkeylog = undefined;
    } else {
      validateFunction(fn, 'onkeylog');
      this.#onkeylog = FunctionPrototypeBind(fn, this);
    }
  }

  get onqlog() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onqlog;
  }

  set onqlog(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onqlog = undefined;
    } else {
      validateFunction(fn, 'onqlog');
      this.#onqlog = FunctionPrototypeBind(fn, this);
      // Flush any qlog entries that were cached before the callback was set.
      if (this.#pendingQlog !== undefined) {
        const pending = this.#pendingQlog;
        this.#pendingQlog = undefined;
        for (let i = 0; i < pending.length; i += 2) {
          this[kQlog](pending[i], pending[i + 1]);
        }
      }
    }
  }

  /** @type {Function|undefined} */
  get onsessionticket() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onsessionticket;
  }

  set onsessionticket(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onsessionticket = undefined;
      this.#state.hasSessionTicketListener = false;
    } else {
      validateFunction(fn, 'onsessionticket');
      this.#onsessionticket = FunctionPrototypeBind(fn, this);
      this.#state.hasSessionTicketListener = true;
    }
  }

  /** @type {Function|undefined} */
  get onversionnegotiation() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onversionnegotiation;
  }

  set onversionnegotiation(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onversionnegotiation = undefined;
    } else {
      validateFunction(fn, 'onversionnegotiation');
      this.#onversionnegotiation = FunctionPrototypeBind(fn, this);
    }
  }

  /** @type {Function|undefined} */
  get onhandshake() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onhandshake;
  }

  set onhandshake(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onhandshake = undefined;
    } else {
      validateFunction(fn, 'onhandshake');
      this.#onhandshake = FunctionPrototypeBind(fn, this);
    }
  }

  /** @type {Function|undefined} */
  get onnewtoken() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onnewtoken;
  }

  set onnewtoken(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onnewtoken = undefined;
      this.#state.hasNewTokenListener = false;
    } else {
      validateFunction(fn, 'onnewtoken');
      this.#onnewtoken = FunctionPrototypeBind(fn, this);
      this.#state.hasNewTokenListener = true;
    }
  }

  /** @type {Function|undefined} */
  get onearlyrejected() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onearlyrejected;
  }

  set onearlyrejected(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onearlyrejected = undefined;
    } else {
      validateFunction(fn, 'onearlyrejected');
      this.#onearlyrejected = FunctionPrototypeBind(fn, this);
    }
  }

  /** @type {Function|undefined} */
  get onorigin() {
    QuicSession.#assertIsQuicSession(this);
    return this.#onorigin;
  }

  set onorigin(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#onorigin = undefined;
      this.#state.hasOriginListener = false;
    } else {
      validateFunction(fn, 'onorigin');
      this.#onorigin = FunctionPrototypeBind(fn, this);
      this.#state.hasOriginListener = true;
    }
  }

  /** @type {Function|undefined} */
  get ongoaway() {
    QuicSession.#assertIsQuicSession(this);
    return this.#ongoaway;
  }

  set ongoaway(fn) {
    QuicSession.#assertIsQuicSession(this);
    if (fn === undefined) {
      this.#ongoaway = undefined;
    } else {
      validateFunction(fn, 'ongoaway');
      this.#ongoaway = FunctionPrototypeBind(fn, this);
    }
  }

  /**
   * The maximum datagram size the peer will accept, or 0 if datagrams
   * are not supported or the handshake has not yet completed.
   * @type {bigint}
   */
  get maxDatagramSize() {
    QuicSession.#assertIsQuicSession(this);
    return this.#state.maxDatagramSize;
  }

  /**
   * Maximum number of datagrams that can be queued while inside a
   * ngtcp2 callback scope. When the queue is full, the oldest
   * datagram is dropped and reported as lost. Default is 128.
   * @type {number}
   */
  get maxPendingDatagrams() {
    QuicSession.#assertIsQuicSession(this);
    return this.#state.maxPendingDatagrams;
  }

  set maxPendingDatagrams(val) {
    QuicSession.#assertIsQuicSession(this);
    validateInteger(val, 'maxPendingDatagrams', 0, 0xFFFF);
    this.#state.maxPendingDatagrams = val;
  }

  /**
   * The statistics collected for this session.
   * @type {QuicSessionStats}
   */
  get stats() {
    QuicSession.#assertIsQuicSession(this);
    return this.#stats;
  }

  /**
   * The endpoint this session belongs to. If the session has been destroyed,
   * `null` will be returned.
   * @type {QuicEndpoint|null}
   */
  get endpoint() {
    QuicSession.#assertIsQuicSession(this);
    if (this.destroyed) return null;
    return this.#endpoint;
  }

  /**
   * The local and remote socket addresses associated with the session.
   * @type {{ local: SocketAddress, remote: SocketAddress } | undefined}
   */
  get path() {
    QuicSession.#assertIsQuicSession(this);
    if (this.destroyed) return undefined;
    return this.#path ??= {
      __proto__: null,
      local: new InternalSocketAddress(this.#handle.getLocalAddress()),
      remote: new InternalSocketAddress(this.#handle.getRemoteAddress()),
    };
  }

  /**
   * The local certificate as an object, or undefined if not available.
   * @type {object|undefined}
   */
  get certificate() {
    QuicSession.#assertIsQuicSession(this);
    if (this.destroyed) return undefined;
    return this.#certificate ??= this.#handle.getCertificate();
  }

  /**
   * The peer's certificate as an object, or undefined if the peer did
   * not present a certificate or the session is destroyed.
   * @type {object|undefined}
   */
  get peerCertificate() {
    QuicSession.#assertIsQuicSession(this);
    if (this.destroyed) return undefined;
    return this.#peerCertificate ??= this.#handle.getPeerCertificate();
  }

  /**
   * The ephemeral key info for the session. Only available on client
   * sessions. Returns undefined for server sessions or if the session
   * is destroyed.
   * @type {object|undefined}
   */
  get ephemeralKeyInfo() {
    QuicSession.#assertIsQuicSession(this);
    if (this.destroyed) return undefined;
    return this.#ephemeralKeyInfo ??= this.#handle.getEphemeralKey();
  }

  /**
   * @param {number} direction
   * @param {OpenStreamOptions} options
   * @returns {QuicStream}
   */
  async #createStream(direction, options = kEmptyObject) {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed. New streams cannot be opened.');
    }
    const dir = direction === kStreamDirectionBidirectional ? 'bidi' : 'uni';
    if (this.#state.isStreamOpenAllowed) {
      debug(`opening new pending ${dir} stream`);
    } else {
      debug(`opening new ${dir} stream`);
    }

    validateObject(options, 'options');
    const {
      body,
      priority = 'default',
      incremental = false,
      highWaterMark = kDefaultHighWaterMark,
      headers,
      onheaders,
      ontrailers,
      oninfo,
      onwanttrailers,
    } = options;

    validateOneOf(priority, 'options.priority', ['default', 'low', 'high']);
    validateBoolean(incremental, 'options.incremental');

    const validatedBody = validateBody(body);

    const handle = this.#handle.openStream(direction, validatedBody);
    if (handle === undefined) {
      throw new ERR_QUIC_OPEN_STREAM_FAILED();
    }

    if (this.#state.isPrioritySupported) {
      const urgency = priority === 'high' ? 0 : priority === 'low' ? 7 : 3;
      handle.setPriority((urgency << 1) | (incremental ? 1 : 0));
    }

    const stream = new QuicStream(kPrivateConstructor, handle, this, direction);
    this.#streams.add(stream);
    if (typeof this.#onerror === 'function') {
      markPromiseAsHandled(stream.closed);
    }

    // If the body was a FileHandle, store it on the stream so it is
    // closed automatically when the stream finishes.
    if (body instanceof FileHandle) {
      stream[kAttachFileHandle](body);
    }

    // Set the high water mark for backpressure.
    stream.highWaterMark = highWaterMark;

    // Set stream callbacks before sending headers to avoid missing events.
    if (onheaders) stream.onheaders = onheaders;
    if (ontrailers) stream.ontrailers = ontrailers;
    if (oninfo) stream.oninfo = oninfo;
    if (onwanttrailers) stream.onwanttrailers = onwanttrailers;

    if (headers !== undefined) {
      stream.sendHeaders(headers, { terminal: validatedBody === undefined });
    }

    if (onSessionOpenStreamChannel.hasSubscribers) {
      onSessionOpenStreamChannel.publish({
        __proto__: null,
        stream,
        session: this,
        direction: dir,
      });
    }
    return stream;
  }

  /**
   * Creates a new bidirectional stream on this session. If the session
   * does not allow new streams to be opened, an error will be thrown.
   * @param {OpenStreamOptions} [options]
   * @returns {Promise<QuicStream>}
   */
  async createBidirectionalStream(options = kEmptyObject) {
    QuicSession.#assertIsQuicSession(this);
    return await this.#createStream(kStreamDirectionBidirectional, options);
  }

  /**
   * Creates a new unidirectional stream on this session. If the session
   * does not allow new streams to be opened, an error will be thrown.
   * @param {OpenStreamOptions} [options]
   * @returns {Promise<QuicStream>}
   */
  async createUnidirectionalStream(options = kEmptyObject) {
    QuicSession.#assertIsQuicSession(this);
    return await this.#createStream(kStreamDirectionUnidirectional, options);
  }

  /**
   * Send a datagram. The id of the sent datagram will be returned. The status
   * of the sent datagram will be reported via the datagram-status event if
   * possible.
   *
   * If a string is given it will be encoded using the specified encoding.
   *
   * If an ArrayBufferView is given, the bytes are copied into an internal
   * buffer; the caller's source buffer is unchanged and may be reused
   * immediately. Callers that want to ensure their source cannot be
   * mutated after the call (for example, when handing the buffer off to
   * another async consumer) can call ArrayBuffer.prototype.transfer()
   * themselves before passing it.
   *
   * If a Promise is given, it will be awaited before sending. If the
   * session closes while awaiting, 0n is returned silently.
   * @param {ArrayBufferView|string|Promise} datagram The datagram payload
   * @param {string} [encoding] The encoding to use if datagram is a string
   * @returns {Promise<bigint>} The datagram ID
   */
  async sendDatagram(datagram, encoding = 'utf8') {
    QuicSession.#assertIsQuicSession(this);
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }

    const maxDatagramSize = this.#state.maxDatagramSize;

    // The peer max datagram size is either unknown or they have explicitly
    // indicated that they do not support datagrams by setting it to 0. In
    // either case, we do not send the datagram.
    if (maxDatagramSize === 0) return kNilDatagramId;

    if (isPromise(datagram)) {
      datagram = await datagram;
      // Session may have closed while awaiting. Since datagrams are
      // inherently unreliable, silently return rather than throwing.
      if (this.#isClosedOrClosing) return kNilDatagramId;
    }

    if (typeof datagram === 'string') {
      datagram = new Uint8Array(Buffer.from(datagram, encoding));
    } else if (!isArrayBufferView(datagram)) {
      throw new ERR_INVALID_ARG_TYPE('datagram',
                                     ['ArrayBufferView', 'string'],
                                     datagram);
    }

    const length = isDataView(datagram) ?
      DataViewPrototypeGetByteLength(datagram) :
      TypedArrayPrototypeGetByteLength(datagram);

    // If the view has zero length (e.g. detached buffer), there's
    // nothing to send.
    if (length === 0) return kNilDatagramId;

    // The peer max datagram size is less than the datagram we want to send,
    // so... don't send it.
    if (length > maxDatagramSize) return kNilDatagramId;

    const id = this.#handle.sendDatagram(datagram);

    if (id !== kNilDatagramId && onSessionSendDatagramChannel.hasSubscribers) {
      onSessionSendDatagramChannel.publish({
        __proto__: null,
        id,
        length,
        session: this,
      });
    }

    debug(`datagram ${id} sent with ${length} bytes`);
    return id;
  }

  /**
   * Initiate a key update.
   */
  updateKey() {
    QuicSession.#assertIsQuicSession(this);
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }

    debug('updating session key');

    this.#handle.updateKey();
    if (onSessionUpdateKeyChannel.hasSubscribers) {
      onSessionUpdateKeyChannel.publish({
        __proto__: null,
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
   * @param {object} [options]
   * @param {bigint|number} [options.code] The error code to send in the
   *   CONNECTION_CLOSE frame. Defaults to NO_ERROR (0).
   * @param {string} [options.type] Either `'transport'` (default) or
   *   `'application'`. Determines the error code namespace.
   * @param {string} [options.reason] An optional human-readable reason
   *   string included in the CONNECTION_CLOSE frame (diagnostic only).
   * @returns {Promise<void>}
   */
  close(options = kEmptyObject) {
    QuicSession.#assertIsQuicSession(this);
    options = validateCloseOptions(options);
    if (!this.#isClosedOrClosing) {
      this.#isPendingClose = true;
      if (options?.code !== undefined) {
        this.#selfInitiatedClose = true;
      }

      debug('gracefully closing the session');

      this.#handle.gracefulClose(options);
      if (onSessionClosingChannel.hasSubscribers) {
        onSessionClosingChannel.publish({
          __proto__: null,
          session: this,
        });
      }
    }
    return this.closed;
  }

  /** @type {boolean} */
  get closing() {
    return this.#isPendingClose;
  }

  /** @type {Promise<QuicSessionInfo>} */
  get opened() {
    QuicSession.#assertIsQuicSession(this);
    return this.#pendingOpen.promise;
  }

  /**
   * A promise that is resolved when the session is closed, or is rejected if
   * the session is closed abruptly due to an error.
   * @type {Promise<void>}
   */
  get closed() {
    QuicSession.#assertIsQuicSession(this);
    return this.#pendingClose.promise;
  }

  /** @type {boolean} */
  get destroyed() {
    QuicSession.#assertIsQuicSession(this);
    return this.#handle === undefined;
  }

  /**
   * Forcefully closes the session abruptly without waiting for streams to be
   * completed naturally. Any streams that are still open will be immediately
   * destroyed and any queued datagrams will be dropped. If an error is given,
   * the closed promise will be rejected with that error. If no error is given,
   * the closed promise will be resolved.
   * @param {any} error
   * @param {object} [options]
   * @param {bigint|number} [options.code] The error code to send in the
   *   CONNECTION_CLOSE frame. Defaults to NO_ERROR (0).
   * @param {string} [options.type] Either `'transport'` (default) or
   *   `'application'`. Determines the error code namespace.
   * @param {string} [options.reason] An optional human-readable reason
   *   string included in the CONNECTION_CLOSE frame (diagnostic only).
   */
  destroy(error, options) {
    QuicSession.#assertIsQuicSession(this);
    // Two distinct guards (see also `QuicStream.destroy`):
    //   * `#destroying` flips synchronously here so any re-entrant call
    //     (e.g. from a user `onerror` callback or from a cascading
    //     `stream.destroy(error)` whose own `onerror` re-enters
    //     `session.destroy()`) hits this guard and returns immediately
    //     without running the teardown twice.
    //   * `destroyed` (i.e. `#handle === undefined`) signals
    //     "fully torn down". Defense-in-depth for paths that may have
    //     finished teardown without setting `#destroying` and for
    //     repeat invocations after this method has fully run.
    if (this.#destroying || this.destroyed) return;

    if (options !== undefined) options = validateCloseOptions(options);
    this.#destroying = true;

    debug('destroying the session');

    if (error !== undefined) {
      if (onSessionErrorChannel.hasSubscribers) {
        onSessionErrorChannel.publish({
          __proto__: null,
          session: this,
          error,
        });
      }
      if (typeof this.#onerror === 'function') {
        invokeOnerror(this.#onerror, error);
      }
    }

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

    // If the handshake never completed, reject the opened promise. The
    // session is being destroyed, so the handshake will never complete
    // and `await session.opened` would otherwise hang forever. The
    // documented contract is that opened rejects when the session is
    // destroyed before opening; see the `session.opened` docs in
    // doc/api/quic.md. `[kHandshake]` clears `#pendingOpen.reject` once
    // the handshake completes successfully, so this branch only runs if
    // we are racing against a still-pending handshake.
    //
    // Mark the rejection as handled before rejecting so that callers who
    // never explicitly `await session.opened` do not get an unhandled
    // rejection warning - common for server-side sessions delivered via
    // `onsession`, which often do not await opened. The rejection is
    // still observable via `await session.opened`.
    if (this.#pendingOpen.reject) {
      markPromiseAsHandled(this.#pendingOpen.promise);
      this.#pendingOpen.reject(error ?? new ERR_INVALID_STATE(
        'Session was destroyed before it opened'));
    }

    if (error) {
      // If the session is still waiting to be closed, and error
      // is specified, reject the closed promise.
      this.#pendingClose.reject?.(error);
    } else {
      this.#pendingClose.resolve?.();
    }

    this.#pendingClose.reject = undefined;
    this.#pendingClose.resolve = undefined;
    this.#pendingOpen.reject = undefined;
    this.#pendingOpen.resolve = undefined;

    this.#state[kFinishClose]();
    this.#stats[kFinishClose]();

    if (this[kPerfEntry] && hasObserver('quic')) {
      stopPerf(this, kPerfEntry, {
        detail: {
          stats: this.stats,
          handshake: this.#handshakeInfo,
          path: this.#path,
        },
      });
    }

    this.#onerror = undefined;
    this.#onstream = undefined;
    this.#ondatagram = undefined;
    this.#ondatagramstatus = undefined;
    this.#onpathvalidation = undefined;
    this.#onsessionticket = undefined;
    this.#onkeylog = undefined;
    this.#onversionnegotiation = undefined;
    this.#onhandshake = undefined;
    this.#onnewtoken = undefined;
    this.#onorigin = undefined;
    this.#ongoaway = undefined;
    this.#path = undefined;
    this.#certificate = undefined;
    this.#peerCertificate = undefined;
    this.#ephemeralKeyInfo = undefined;

    // Destroy the underlying C++ handle. Pass close error options if
    // provided so the CONNECTION_CLOSE frame carries the correct code.
    // Note: #onqlog is intentionally NOT cleared here because ngtcp2
    // emits the final qlog statement during ngtcp2_conn destruction,
    // and the deferred callback must still be reachable. The reference
    // is released when the QuicSession object is garbage collected.
    this.#handle.destroy(options);
    this.#handle = undefined;

    if (onSessionClosedChannel.hasSubscribers) {
      onSessionClosedChannel.publish({
        __proto__: null,
        session: this,
        error,
        stats: this.stats,
      });
    }
  }

  /**
   * Called when the peer sends a GOAWAY frame (HTTP/3 only). The
   * lastStreamId indicates the highest stream ID the peer may have
   * processed - streams above it were not processed and may be retried.
   * @param {bigint} lastStreamId
   */
  [kGoaway](lastStreamId) {
    this.#isPendingClose = true;
    if (onSessionClosingChannel.hasSubscribers) {
      onSessionClosingChannel.publish({ __proto__: null, session: this });
    }
    if (onSessionGoawayChannel.hasSubscribers) {
      onSessionGoawayChannel.publish({
        __proto__: null,
        session: this,
        lastStreamId,
      });
    }
    if (this.#ongoaway) {
      safeCallbackInvoke(this.#ongoaway, this, lastStreamId);
    }
  }

  /**
   * @param {number} errorType
   * @param {number} code
   * @param {string} [reason]
   */
  [kFinishClose](errorType, code, reason) {
    // If code is zero, then we closed without an error. Yay! We can destroy
    // safely without specifying an error.
    if (code === 0n) {
      debug('finishing closing the session with no error');
      this.destroy();
      return;
    }

    debug('finishing closing the session with an error', errorType, code, reason);

    // If the local side initiated this close with an error code (via
    // close({ code })), this is an intentional shutdown; not an error.
    // The closed promise should resolve, not reject.
    if (this.#selfInitiatedClose) {
      this.destroy();
      return;
    }

    // Otherwise, errorType indicates the type of error that occurred, code indicates
    // the specific error, and reason is an optional string describing the error.
    // code !== 0n here (the early return above handles code === 0n).
    // The errorType values map to ngtcp2_ccerr_type:
    //   0 = NGTCP2_CCERR_TYPE_TRANSPORT
    //   1 = NGTCP2_CCERR_TYPE_APPLICATION
    //   2 = NGTCP2_CCERR_TYPE_VERSION_NEGOTIATION
    //   3 = NGTCP2_CCERR_TYPE_IDLE_CLOSE
    //   4 = NGTCP2_CCERR_TYPE_DROP_CONN
    //   5 = NGTCP2_CCERR_TYPE_RETRY
    // The DROP_CONN/RETRY cases are typically intercepted before reaching
    // here (DROP_CONN tears the connection down without notifying us, RETRY
    // is server-only). The default branch is a safety net so any
    // unexpected value still completes the close path - without it the
    // session would leak with `closed` hanging forever.
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
      case 3: /* Idle close */
        this.destroy();
        break;
      default:
        this.destroy(new ERR_QUIC_TRANSPORT_ERROR(code, reason));
        break;
    }
  }

  [kKeylog](line) {
    if (this.destroyed || this.onkeylog === undefined) return;
    safeCallbackInvoke(this.#onkeylog, this, line);
  }

  [kQlog](data, fin) {
    if (this.onqlog === undefined) return;
    safeCallbackInvoke(this.#onqlog, this, data, fin);
  }

  /**
   * @param {Uint8Array} u8 The datagram payload
   * @param {boolean} early A boolean indicating whether this datagram was received before the handshake completed
   */
  [kDatagram](u8, early) {
    // The datagram event should only be called if the session has
    // an ondatagram callback. The callback should always exist here.
    assert(typeof this.#ondatagram === 'function', 'Unexpected datagram event');
    if (this.destroyed) return;
    const length = TypedArrayPrototypeGetByteLength(u8);
    if (onSessionReceiveDatagramChannel.hasSubscribers) {
      onSessionReceiveDatagramChannel.publish({
        __proto__: null,
        length,
        early,
        session: this,
      });
    }
    safeCallbackInvoke(this.#ondatagram, this, u8, early);
  }

  /**
   * @param {bigint} id
   * @param {'lost'|'acknowledged'} status
   */
  [kDatagramStatus](id, status) {
    // The datagram status event should only be called if the session has
    // an ondatagramstatus callback. The callback should always exist here.
    assert(typeof this.#ondatagramstatus === 'function', 'Unexpected datagram status event');
    if (this.destroyed) return;
    if (onSessionReceiveDatagramStatusChannel.hasSubscribers) {
      onSessionReceiveDatagramStatusChannel.publish({
        __proto__: null,
        id,
        status,
        session: this,
      });
    }
    safeCallbackInvoke(this.#ondatagramstatus, this, id, status);
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
    assert(typeof this.#onpathvalidation === 'function',
           'Unexpected path validation event');
    if (this.destroyed) return;
    const newLocal = new InternalSocketAddress(newLocalAddress);
    const newRemote = new InternalSocketAddress(newRemoteAddress);
    const oldLocal = oldLocalAddress !== undefined ?
      new InternalSocketAddress(oldLocalAddress) : null;
    const oldRemote = oldRemoteAddress !== undefined ?
      new InternalSocketAddress(oldRemoteAddress) : null;
    if (onSessionPathValidationChannel.hasSubscribers) {
      onSessionPathValidationChannel.publish({
        __proto__: null,
        result,
        newLocalAddress: newLocal,
        newRemoteAddress: newRemote,
        oldLocalAddress: oldLocal,
        oldRemoteAddress: oldRemote,
        preferredAddress,
        session: this,
      });
    }
    safeCallbackInvoke(this.#onpathvalidation, this, result, newLocal, newRemote,
                       oldLocal, oldRemote, preferredAddress);
  }

  /**
   * @param {object} ticket
   */
  [kSessionTicket](ticket) {
    assert(typeof this.#onsessionticket === 'function',
           'Unexpected session ticket event');
    if (this.destroyed) return;
    if (onSessionTicketChannel.hasSubscribers) {
      onSessionTicketChannel.publish({
        __proto__: null,
        ticket,
        session: this,
      });
    }
    safeCallbackInvoke(this.#onsessionticket, this, ticket);
  }

  /**
   * @param {Buffer} token
   * @param {SocketAddress} address
   */
  [kNewToken](token, address) {
    assert(typeof this.#onnewtoken === 'function',
           'Unexpected new token event');
    if (this.destroyed) return;
    const addr = new InternalSocketAddress(address);
    if (onSessionNewTokenChannel.hasSubscribers) {
      onSessionNewTokenChannel.publish({
        __proto__: null,
        token,
        address: addr,
        session: this,
      });
    }
    safeCallbackInvoke(this.#onnewtoken, this, token, addr);
  }

  [kEarlyDataRejected]() {
    if (this.destroyed) return;
    if (onSessionEarlyRejectedChannel.hasSubscribers) {
      onSessionEarlyRejectedChannel.publish({
        __proto__: null,
        session: this,
      });
    }
    if (typeof this.#onearlyrejected === 'function') {
      safeCallbackInvoke(this.#onearlyrejected, this);
    }
  }

  /**
   * @param {number} version
   * @param {number[]} requestedVersions
   * @param {number[]} supportedVersions
   */
  [kVersionNegotiation](version, requestedVersions, supportedVersions) {
    if (this.destroyed) return;
    if (onSessionVersionNegotiationChannel.hasSubscribers) {
      onSessionVersionNegotiationChannel.publish({
        __proto__: null,
        version,
        requestedVersions,
        supportedVersions,
        session: this,
      });
    }
    if (this.#onversionnegotiation) {
      safeCallbackInvoke(this.#onversionnegotiation, this,
                         version, requestedVersions, supportedVersions);
    }
    // Version negotiation is always a fatal event - the session must be
    // destroyed regardless of whether the callback is set.
    this.destroy(new ERR_QUIC_VERSION_NEGOTIATION_ERROR());
  }

  /**
   * Called when the session receives an ORIGIN frame (RFC 9412).
   * @param {string[]} origins
   */
  [kOrigin](origins) {
    assert(typeof this.#onorigin === 'function',
           'Unexpected origin event');
    if (this.destroyed) return;
    if (onSessionOriginChannel.hasSubscribers) {
      onSessionOriginChannel.publish({
        __proto__: null,
        origins,
        session: this,
      });
    }
    safeCallbackInvoke(this.#onorigin, this, origins);
  }

  /**
   * @param {string} servername
   * @param {string} protocol
   * @param {string} cipher
   * @param {string} cipherVersion
   * @param {string} validationErrorReason
   * @param {number} validationErrorCode
   */
  [kHandshake](servername, protocol, cipher, cipherVersion, validationErrorReason,
               validationErrorCode, earlyDataAttempted, earlyDataAccepted) {
    if (this.destroyed || !this.#pendingOpen.resolve) return;

    const addr = this.#handle.getRemoteAddress();

    const info = {
      __proto__: null,
      local: this.#endpoint.address,
      remote: addr !== undefined ?
        new InternalSocketAddress(addr) :
        undefined,
      servername,
      protocol,
      cipher,
      cipherVersion,
      validationErrorReason,
      validationErrorCode,
      earlyDataAttempted,
      earlyDataAccepted,
    };

    // Stash timing-relevant handshake info for the perf entry detail.
    this.#handshakeInfo = {
      __proto__: null,
      servername,
      protocol,
      earlyDataAttempted,
      earlyDataAccepted,
    };

    if (onSessionHandshakeChannel.hasSubscribers) {
      onSessionHandshakeChannel.publish({
        __proto__: null,
        session: this,
        ...info,
      });
    }

    if (this.#onhandshake) {
      safeCallbackInvoke(this.#onhandshake, this, info);
    }

    this.#pendingOpen.resolve?.(info);
    this.#pendingOpen.resolve = undefined;
    this.#pendingOpen.reject = undefined;
    this.#handshakeCompleted = true;
  }

  /** @type {boolean} */
  get [kHandshakeCompleted]() {
    return this.#handshakeCompleted;
  }

  /**
   * @param {object} handle
   * @param {number} direction
   */
  [kNewStream](handle, direction) {
    const stream = new QuicStream(kPrivateConstructor, handle, this, direction);

    // Set the default high water mark for received streams.
    stream.highWaterMark = kDefaultHighWaterMark;

    // A new stream was received. If we don't have an onstream callback, then
    // there's nothing we can do about it. Destroy the stream in this case.
    if (typeof this.#onstream !== 'function') {
      process.emitWarning('A new stream was received but no onstream callback was provided');
      stream.destroy();
      return;
    }
    this.#streams.add(stream);
    // If the session has an onerror handler, mark the stream's closed
    // promise as handled. See the onerror setter for explanation.
    if (typeof this.#onerror === 'function') {
      markPromiseAsHandled(stream.closed);
    }

    // Apply default stream callbacks set at listen time before
    // notifying onstream, so the user sees them already set.
    const scbs = this[kStreamCallbacks];
    if (scbs) {
      if (scbs.onheaders) stream.onheaders = scbs.onheaders;
      if (scbs.ontrailers) stream.ontrailers = scbs.ontrailers;
      if (scbs.oninfo) stream.oninfo = scbs.oninfo;
      if (scbs.onwanttrailers) stream.onwanttrailers = scbs.onwanttrailers;
    }

    if (onSessionReceivedStreamChannel.hasSubscribers) {
      onSessionReceivedStreamChannel.publish({
        __proto__: null,
        stream,
        session: this,
        direction: direction === kStreamDirectionBidirectional ? 'bidi' : 'uni',
      });
    }

    safeCallbackInvoke(this.#onstream, this, stream);
  }

  [kRemoveStream](stream) {
    this.#streams.delete(stream);
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `QuicSession ${inspect({
      closed: this.closed,
      closing: this.#isPendingClose,
      destroyed: this.destroyed,
      endpoint: this.endpoint,
      path: this.path,
      state: this.#state,
      stats: this.stats,
      streams: this.#streams,
    }, opts)}`;
  }

  async [SymbolAsyncDispose]() { await this.close(); }
}

let isQuicEndpoint;

// The QuicEndpoint represents a local UDP port binding. It can act as both a
// server for receiving peer sessions, or a client for initiating them. The
// local UDP port will be lazily bound only when connect() or listen() are
// called.
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
  #pendingClose = PromiseWithResolvers();
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
  #onsession = undefined;
  #sessionCallbacks = undefined;

  static {
    getQuicEndpointState = function(endpoint) {
      assertIsQuicEndpoint(endpoint);
      return endpoint.#state;
    };

    isQuicEndpoint = function(val) {
      return val != null && #handle in val;
    };

    assertIsQuicEndpoint = function(val) {
      if (!isQuicEndpoint(val)) {
        throw new ERR_INVALID_THIS('QuicEndpoint');
      }
    };

    assertEndpointNotClosedOrClosing = function(endpoint) {
      if (endpoint.#isClosedOrClosing) {
        throw new ERR_INVALID_STATE('Endpoint is closed');
      }
    };

    assertEndpointIsNotBusy = function(endpoint) {
      if (endpoint.#state.isBusy) {
        throw new ERR_INVALID_STATE('Endpoint is busy');
      }
    };
  }

  /**
   * @param {EndpointOptions} options
   * @returns {EndpointOptions}
   */
  #processEndpointOptions(options) {
    validateObject(options, 'options');
    let { address } = options;
    const {
      retryTokenExpiration,
      tokenExpiration,
      maxConnectionsPerHost = 0,
      maxConnectionsTotal = 0,
      maxStatelessResetsPerHost,
      disableStatelessReset,
      addressLRUSize,
      maxRetries,
      rxDiagnosticLoss,
      txDiagnosticLoss,
      udpReceiveBufferSize,
      udpSendBufferSize,
      udpTTL,
      idleTimeout,
      validateAddress,
      ipv6Only,
      cc,
      resetTokenSecret,
      tokenSecret,
    } = options;

    // All of the other options will be validated internally by the C++ code
    if (address !== undefined && !SocketAddress.isSocketAddress(address)) {
      if (typeof address === 'string') {
        address = SocketAddress.parse(address);
      } else if (typeof address === 'object' && address !== null) {
        address = new SocketAddress(address);
      } else {
        throw new ERR_INVALID_ARG_TYPE('options.address', ['SocketAddress', 'string'], address);
      }
    }

    return {
      __proto__: null,
      address: address?.[kSocketAddressHandle],
      retryTokenExpiration,
      tokenExpiration,
      // Connection limits are set on the state buffer, not passed to C++.
      maxConnectionsPerHost,
      maxConnectionsTotal,
      maxStatelessResetsPerHost,
      disableStatelessReset,
      addressLRUSize,
      maxRetries,
      rxDiagnosticLoss,
      txDiagnosticLoss,
      udpReceiveBufferSize,
      udpSendBufferSize,
      udpTTL,
      idleTimeout,
      validateAddress,
      ipv6Only,
      cc,
      resetTokenSecret,
      tokenSecret,
    };
  }

  #newSession(handle) {
    const session = new QuicSession(kPrivateConstructor, handle, this);
    this.#sessions.add(session);
    // Set default pending datagram queue size.
    session.maxPendingDatagrams = kDefaultMaxPendingDatagrams;
    return session;
  }

  /**
   * @param {EndpointOptions} config
   */
  constructor(config = kEmptyObject) {
    const options = this.#processEndpointOptions(config);
    this.#handle = new Endpoint_(options);
    this.#handle[kOwner] = this;
    this.#stats = new QuicEndpointStats(kPrivateConstructor, this.#handle.stats);
    this.#state = new QuicEndpointState(kPrivateConstructor, this.#handle.state);

    // Connection limits are stored in the shared state buffer so they
    // can be read by C++ and mutated from JS after construction.
    // Use the public setters which validate the range.
    if (options.maxConnectionsPerHost !== undefined) {
      this.maxConnectionsPerHost = options.maxConnectionsPerHost;
    }
    if (options.maxConnectionsTotal !== undefined) {
      this.maxConnectionsTotal = options.maxConnectionsTotal;
    }

    endpointRegistry.add(this);

    if (hasObserver('quic')) {
      startPerf(this, kPerfEntry, { type: 'quic', name: 'QuicEndpoint' });
    }

    if (onEndpointCreatedChannel.hasSubscribers) {
      onEndpointCreatedChannel.publish({
        __proto__: null,
        endpoint: this,
        config,
      });
    }

    debug('endpoint created');
  }

  /**
   * Statistics collected while the endpoint is operational.
   * @type {QuicEndpointStats}
   */
  get stats() {
    assertIsQuicEndpoint(this);
    return this.#stats;
  }

  get #isClosedOrClosing() {
    return this.destroyed || this.#isPendingClose;
  }

  /**
   * When an endpoint is marked as busy, it will not accept new connections.
   * Existing connections will continue to work.
   * @type {boolean}
   */
  get busy() {
    assertIsQuicEndpoint(this);
    return this.#busy;
  }

  /**
   * @type {boolean}
   */
  set busy(val) {
    assertIsQuicEndpoint(this);
    assertEndpointNotClosedOrClosing(this);
    // The val is allowed to be any truthy value
    // Non-op if there is no change
    if (!!val !== this.#busy) {
      debug('toggling endpoint busy status to ', !this.#busy);
      this.#busy = !this.#busy;
      this.#handle.markBusy(this.#busy);
      if (onEndpointBusyChangeChannel.hasSubscribers) {
        onEndpointBusyChangeChannel.publish({
          __proto__: null,
          endpoint: this,
          busy: this.#busy,
        });
      }
    }
  }

  /**
   * Maximum concurrent connections per remote IP address.
   * 0 means unlimited (default).
   * @type {number}
   */
  get maxConnectionsPerHost() {
    assertIsQuicEndpoint(this);
    return this.#state.maxConnectionsPerHost;
  }

  set maxConnectionsPerHost(val) {
    assertIsQuicEndpoint(this);
    validateInteger(val, 'maxConnectionsPerHost', 0, 0xFFFF);
    this.#state.maxConnectionsPerHost = val;
  }

  /**
   * Maximum total concurrent connections.
   * 0 means unlimited (default).
   * @type {number}
   */
  get maxConnectionsTotal() {
    assertIsQuicEndpoint(this);
    return this.#state.maxConnectionsTotal;
  }

  set maxConnectionsTotal(val) {
    assertIsQuicEndpoint(this);
    validateInteger(val, 'maxConnectionsTotal', 0, 0xFFFF);
    this.#state.maxConnectionsTotal = val;
  }

  /**
   * The local address the endpoint is bound to (if any)
   * @type {SocketAddress|undefined}
   */
  get address() {
    assertIsQuicEndpoint(this);
    if (this.#isClosedOrClosing) return undefined;
    if (this.#address === undefined) {
      const addr = this.#handle.address();
      if (addr !== undefined) this.#address = new InternalSocketAddress(addr);
    }
    return this.#address;
  }

  /**
   * Configures the endpoint to listen for incoming connections.
   * @param {OnSessionCallback|SessionOptions} [onsession]
   * @param {SessionOptions} [options]
   */
  [kListen](onsession, options) {
    assertEndpointNotClosedOrClosing(this);
    assertEndpointIsNotBusy(this);
    if (this.#listening) {
      throw new ERR_INVALID_STATE('Endpoint is already listening');
    }
    validateObject(options, 'options');
    this.#onsession = FunctionPrototypeBind(onsession, this);

    const {
      onerror,
      onstream,
      ondatagram,
      ondatagramstatus,
      onpathvalidation,
      onsessionticket,
      onversionnegotiation,
      onhandshake,
      onnewtoken,
      onearlyrejected,
      onorigin,
      ongoaway,
      onkeylog,
      onqlog,
      // Stream-level callbacks applied to each incoming stream.
      onheaders,
      ontrailers,
      oninfo,
      onwanttrailers,
      ...rest
    } = options;

    // Store session and stream callbacks to apply to each new incoming session.
    this.#sessionCallbacks = {
      __proto__: null,
      onerror,
      onstream,
      ondatagram,
      ondatagramstatus,
      onpathvalidation,
      onsessionticket,
      onversionnegotiation,
      onhandshake,
      onnewtoken,
      onearlyrejected,
      onorigin,
      ongoaway,
      onkeylog,
      onqlog,
      onheaders,
      ontrailers,
      oninfo,
      onwanttrailers,
    };

    debug('endpoint listening as a server');
    this.#handle.listen(rest);
    this.#listening = true;
  }

  /**
   * Initiates a session with a remote endpoint.
   * @param {object} address
   * @param {SessionOptions} [options]
   * @returns {QuicSession}
   */
  [kConnect](address, options) {
    assertEndpointNotClosedOrClosing(this);
    assertEndpointIsNotBusy(this);
    validateObject(options, 'options');
    const {
      sessionTicket,
      ...rest
    } = options;

    debug('endpoint connecting as a client');
    const handle = this.#handle.connect(address, rest, sessionTicket);
    if (handle === undefined) {
      throw new ERR_QUIC_CONNECTION_FAILED();
    }
    const session = this.#newSession(handle);
    // Set callbacks before any async work to avoid missing events
    // that fire during or immediately after the handshake.
    applyCallbacks(session, options);
    return session;
  }

  /**
   * Gracefully closes the endpoint. Any existing sessions will be permitted to
   * end gracefully, after which the endpoint will be closed immediately. New
   * sessions will not be accepted or created. The returned promise will be resolved
   * when closing is complete, or will be rejected if the endpoint is closed abruptly
   * due to an error.
   * @returns {Promise<void>} Returns this.closed
   */
  close() {
    assertIsQuicEndpoint(this);
    if (!this.#isClosedOrClosing) {
      debug('gracefully closing the endpoint');
      if (onEndpointClosingChannel.hasSubscribers) {
        onEndpointClosingChannel.publish({
          __proto__: null,
          endpoint: this,
          hasPendingError: this.#pendingError !== undefined,
        });
      }
      this.#isPendingClose = true;
      this.#handle.closeGracefully();
    }
    return this.closed;
  }

  /**
   * Returns a promise that is resolved when the endpoint is closed or rejects
   * if the endpoint is closed abruptly due to an error. The closed property
   * is set to the same promise that is returned by the close() method.
   * @type {Promise<void>}
   */
  get closed() {
    assertIsQuicEndpoint(this);
    return this.#pendingClose.promise;
  }

  /**
   * True if the endpoint is pending close.
   * @type {boolean}
   */
  get closing() {
    assertIsQuicEndpoint(this);
    return this.#isPendingClose;
  }

  /** @type {boolean} */
  get listening() {
    assertIsQuicEndpoint(this);
    return this.#listening;
  }

  /** @type {boolean} */
  get destroyed() {
    assertIsQuicEndpoint(this);
    return this.#handle === undefined;
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
    assertIsQuicEndpoint(this);
    debug('destroying the endpoint');
    // Record the error before deciding whether to initiate a close. If
    // `close()` was already called (e.g. the user kicked off a graceful
    // shutdown and then a fatal error was reported afterwards via
    // `destroy(err)`) we still want that error to surface on
    // `endpoint.closed` rather than being silently swallowed when the
    // last in-flight session finishes draining. Only the *first* error
    // is recorded, matching how other Node subsystems handle a
    // double-error race.
    if (error !== undefined) this.#pendingError ??= error;
    // Force all sessions to be abruptly closed *before* signalling the
    // endpoint to close gracefully. The order matters: each session's
    // `destroy(error, options)` asks the C++ side to emit a
    // `CONNECTION_CLOSE` frame via `endpoint.Send(...)`. Once the
    // endpoint has entered its closing state (after `close()`) it
    // can drop those outgoing packets, in which case the peer would
    // never learn of the teardown until its own idle timer fires
    // (pimterry's B8).
    //
    // Important: only pass close options to sessions whose handshake
    // has actually completed. Pre-handshake sessions cannot create a
    // valid CONNECTION_CLOSE packet on the C++ side; the fallback
    // synchronously fires `EmitClose` -> JS `[kFinishClose]` ->
    // `destroy()`, which trips the `#destroying` guard and leaves the
    // C++ side asserting an inconsistent destroyed state.
    const closeOptions = errorToCloseOptions(error);
    for (const session of this.#sessions) {
      // Mark each cascaded session's `closed` as handled before
      // destroying it. This prevents unhandled-rejection warnings when
      // the session is collateral damage from an endpoint-level destroy
      // (e.g. a synchronous throw out of a user `onsession` callback
      // routed through safeCallbackInvoke). The rejection is still
      // observable to any caller that explicitly awaits `session.closed`.
      markPromiseAsHandled(session.closed);
      session.destroy(
        error,
        session[kHandshakeCompleted] ? closeOptions : undefined);
    }
    if (!this.#isClosedOrClosing) {
      // Trigger a graceful close of the endpoint that'll ensure that the
      // endpoint is closed down after all sessions are closed... All
      // sessions were just forcefully destroyed above, so this should
      // resolve promptly with nothing left to drain.
      this.close();
    }
    return this.closed;
  }

  /**
   * Replace or merge SNI TLS contexts for this endpoint. Each entry
   * in the map is a host name to TLS identity options object. If
   * replace is true, the entire SNI map is replaced. Otherwise, the
   * provided entries are merged into the existing map.
   * @param {object} entries
   * @param {{replace?: boolean}} [options]
   */
  setSNIContexts(entries, options = kEmptyObject) {
    assertIsQuicEndpoint(this);
    if (this.#handle === undefined) {
      throw new ERR_INVALID_STATE('Endpoint is destroyed');
    }
    validateObject(entries, 'entries');
    const { replace = false } = options;
    validateBoolean(replace, 'options.replace');

    // Process each entry through the identity options validator,
    // then build a full TLS options object (shared + identity).
    const processed = { __proto__: null };
    for (const hostname of ObjectKeys(entries)) {
      validateString(hostname, 'entries key');
      const identity = processIdentityOptions(entries[hostname],
                                              `entries['${hostname}']`);
      if (identity.keys.length === 0) {
        throw new ERR_MISSING_ARGS(`entries['${hostname}'].keys`);
      }
      if (identity.certs === undefined) {
        throw new ERR_MISSING_ARGS(`entries['${hostname}'].certs`);
      }
      processed[hostname] = identity;
    }

    this.#handle.setSNIContexts(processed, replace);
  }

  [kFinishClose](context, status) {
    if (this.#handle === undefined) return;
    debug('endpoint is finishing close', context, status);
    endpointRegistry.delete(this);
    this.#handle = undefined;
    this.#stats[kFinishClose]();
    this.#state[kFinishClose]();
    if (this[kPerfEntry] && hasObserver('quic')) {
      stopPerf(this, kPerfEntry, {
        detail: { stats: this.stats },
      });
    }
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
    const maybeCloseError = maybeGetCloseError(context, status, this.#pendingError);
    if (maybeCloseError !== undefined) {
      if (onEndpointErrorChannel.hasSubscribers) {
        onEndpointErrorChannel.publish({
          __proto__: null,
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
        __proto__: null,
        endpoint: this,
        stats: this.stats,
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
    // Apply session callbacks stored at listen time before notifying
    // the onsession callback, to avoid missing events that fire
    // during or immediately after the handshake.
    if (this.#sessionCallbacks) {
      applyCallbacks(session, this.#sessionCallbacks);
    }
    if (onEndpointServerSessionChannel.hasSubscribers) {
      onEndpointServerSessionChannel.publish({
        __proto__: null,
        endpoint: this,
        session,
        address: session.path?.remote,
      });
    }
    assert(typeof this.#onsession === 'function',
           'onsession callback not specified');
    // Route through safeCallbackInvoke so that a synchronous throw or a
    // rejected promise from the user's onsession callback destroys this
    // endpoint with the error rather than surfacing as an unhandled
    // exception or unhandled rejection coming out of the C++ -> JS
    // boundary.
    safeCallbackInvoke(this.#onsession, this, session);
  }

  // Called by the QuicSession when it closes to remove itself from
  // the active sessions tracked by the QuicEndpoint.
  [kRemoveSession](session) {
    this.#sessions.delete(session);
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      __proto__: null,
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
      state: this.#state,
    }, opts)}`;
  }

  async [SymbolAsyncDispose]() { await this.close(); }
};

/**
 * Find an existing endpoint from the registry that is suitable for reuse.
 * @param {SocketAddress} [targetAddress] The address the client will connect
 *   to. If provided, endpoints that are listening on that same address are
 *   excluded to prevent CID namespace collisions (the client's initial DCID
 *   association would conflict with the server's session routing on the
 *   same endpoint).
 * @returns {QuicEndpoint|undefined}
 */
function findSuitableEndpoint(targetAddress) {
  for (const endpoint of endpointRegistry) {
    if (!endpoint.destroyed &&
        !endpoint.closing &&
        !endpoint.busy) {
      // Don't reuse an endpoint for a connection to itself.
      if (targetAddress && endpoint.listening && endpoint.address &&
          targetAddress.address === endpoint.address.address &&
          targetAddress.port === endpoint.address.port) {
        continue;
      }
      return endpoint;
    }
  }
  return undefined;
}

/**
 * @param {EndpointOptions|QuicEndpoint|undefined} endpoint
 * @param {boolean} reuseEndpoint
 * @param {boolean} forServer
 * @param {SocketAddress} [targetAddress]
 * @returns {QuicEndpoint}
 */
function processEndpointOption(endpoint,
                               reuseEndpoint = true,
                               forServer = false,
                               targetAddress) {
  if (isQuicEndpoint(endpoint)) {
    // We were given an existing endpoint. Use it as-is.
    return endpoint;
  }
  if (endpoint !== undefined) {
    // We were given endpoint options. If reuse is enabled, we could
    // look for a matching endpoint, but endpoint options imply the
    // caller wants specific configuration. Create a new one.
    return new QuicEndpoint(endpoint);
  }
  // No endpoint specified. Try to reuse an existing one if allowed.
  if (reuseEndpoint && !forServer) {
    const existing = findSuitableEndpoint(targetAddress);
    if (existing !== undefined) return existing;
  }
  return new QuicEndpoint();
}

/**
 * Validate and extract identity options (keys, certs) from an SNI entry.
 * CA and CRL are shared TLS options, not per-identity.
 * @param {object} identity
 * @param {string} label
 * @returns {object}
 */
function processIdentityOptions(identity, label) {
  const {
    keys,
    certs,
    verifyPrivateKey = false,
  } = identity;

  if (certs !== undefined) {
    const certInputs = ArrayIsArray(certs) ? certs : [certs];
    for (const cert of certInputs) {
      if (!isArrayBufferView(cert) && !isArrayBuffer(cert)) {
        throw new ERR_INVALID_ARG_TYPE(`${label}.certs`,
                                       ['ArrayBufferView', 'ArrayBuffer'], cert);
      }
    }
  }

  const keyHandles = [];
  if (keys !== undefined) {
    const keyInputs = ArrayIsArray(keys) ? keys : [keys];
    for (const key of keyInputs) {
      if (isKeyObject(key)) {
        if (key.type !== 'private') {
          throw new ERR_INVALID_ARG_VALUE(`${label}.keys`, key,
                                          'must be a private key');
        }
        ArrayPrototypePush(keyHandles, key[kKeyObjectHandle]);
      } else {
        throw new ERR_INVALID_ARG_TYPE(`${label}.keys`, 'KeyObject', key);
      }
    }
  }

  validateBoolean(verifyPrivateKey, `${label}.verifyPrivateKey`);

  return {
    __proto__: null,
    keys: keyHandles,
    certs,
    verifyPrivateKey,
  };
}

/**
 * @param {object} tls
 * @param {boolean} forServer
 * @returns {object}
 */
function processTlsOptions(tls, forServer) {
  const {
    servername,
    alpn,
    ciphers = DEFAULT_CIPHERS,
    groups = DEFAULT_GROUPS,
    keylog = false,
    verifyClient = false,
    rejectUnauthorized = true,
    enableEarlyData = true,
    tlsTrace = false,
    sni,
    // Client-only: identity options are specified directly (no sni map)
    keys,
    certs,
    ca,
    crl,
    verifyPrivateKey = false,
  } = tls;

  if (servername !== undefined) {
    validateString(servername, 'options.servername');
  }
  if (ciphers !== undefined) {
    validateString(ciphers, 'options.ciphers');
  }
  if (groups !== undefined) {
    validateString(groups, 'options.groups');
  }
  validateBoolean(keylog, 'options.keylog');
  validateBoolean(verifyClient, 'options.verifyClient');
  validateBoolean(rejectUnauthorized, 'options.rejectUnauthorized');
  validateBoolean(enableEarlyData, 'options.enableEarlyData');
  validateBoolean(tlsTrace, 'options.tlsTrace');

  // Encode the ALPN option to wire format (length-prefixed protocol names).
  // Server: array of protocol names. Client: single protocol name.
  // If not specified, the C++ default (h3) is used.
  let encodedAlpn;
  if (alpn !== undefined) {
    const protocols = forServer ?
      (ArrayIsArray(alpn) ? alpn : [alpn]) :
      [alpn];
    if (!forServer) {
      validateString(alpn, 'options.alpn');
    }
    let totalLen = 0;
    for (let i = 0; i < protocols.length; i++) {
      validateString(protocols[i], `options.alpn[${i}]`);
      if (protocols[i].length === 0 || protocols[i].length > 255) {
        throw new ERR_INVALID_ARG_VALUE(`options.alpn[${i}]`, protocols[i],
                                        'must be between 1 and 255 characters');
      }
      totalLen += 1 + protocols[i].length;
    }
    // Build wire format: [len1][name1][len2][name2]...
    const buf = Buffer.allocUnsafe(totalLen);
    let offset = 0;
    for (let i = 0; i < protocols.length; i++) {
      buf[offset++] = protocols[i].length;
      buf.write(protocols[i], offset, 'ascii');
      offset += protocols[i].length;
    }
    encodedAlpn = buf.toString('latin1');
  }

  if (ca !== undefined) {
    const caInputs = ArrayIsArray(ca) ? ca : [ca];
    for (const caCert of caInputs) {
      if (!isArrayBufferView(caCert) && !isArrayBuffer(caCert)) {
        throw new ERR_INVALID_ARG_TYPE('options.ca',
                                       ['ArrayBufferView', 'ArrayBuffer'],
                                       caCert);
      }
    }
  }

  if (crl !== undefined) {
    const crlInputs = ArrayIsArray(crl) ? crl : [crl];
    for (const crlCert of crlInputs) {
      if (!isArrayBufferView(crlCert) && !isArrayBuffer(crlCert)) {
        throw new ERR_INVALID_ARG_TYPE('options.crl',
                                       ['ArrayBufferView', 'ArrayBuffer'],
                                       crlCert);
      }
    }
  }

  // Shared TLS options (same for all identities on the endpoint).
  const shared = {
    __proto__: null,
    servername,
    alpn: encodedAlpn,
    ciphers,
    groups,
    keylog,
    verifyClient,
    rejectUnauthorized,
    enableEarlyData,
    tlsTrace,
    ca,
    crl,
  };

  // For servers, identity options come from the sni map.
  // The '*' entry is the optional default/fallback identity. If omitted,
  // only connections with a servername matching a specific entry will
  // succeed; all others will be rejected at the TLS level.
  if (forServer) {
    if (sni === undefined || typeof sni !== 'object') {
      throw new ERR_MISSING_ARGS('options.sni');
    }

    // Must have at least one identity entry (wildcard or hostname-specific).
    // A server with no identity at all cannot serve any connections.
    const sniKeys = ObjectKeys(sni);
    if (sniKeys.length === 0) {
      throw new ERR_MISSING_ARGS('options.sni');
    }

    // Process the default ('*') identity if present.
    let defaultIdentity = {};
    if (sni['*'] !== undefined) {
      defaultIdentity = processIdentityOptions(sni['*'], "options.sni['*']");
      if (defaultIdentity.keys.length === 0) {
        throw new ERR_MISSING_ARGS("options.sni['*'].keys");
      }
      if (defaultIdentity.certs === undefined) {
        throw new ERR_MISSING_ARGS("options.sni['*'].certs");
      }
    }

    // Build the SNI entries (excluding '*') as full TLS options objects.
    // Each inherits the shared options and overrides the identity fields.
    const sniEntries = { __proto__: null };
    for (const hostname of sniKeys) {
      if (hostname === '*') continue;
      validateString(hostname, 'options.sni key');
      const identity = processIdentityOptions(sni[hostname],
                                              `options.sni['${hostname}']`);
      if (identity.keys.length === 0) {
        throw new ERR_MISSING_ARGS(`options.sni['${hostname}'].keys`);
      }
      if (identity.certs === undefined) {
        throw new ERR_MISSING_ARGS(`options.sni['${hostname}'].certs`);
      }
      // Extract ORIGIN frame options from the SNI entry.
      const {
        port,
        authoritative,
      } = sni[hostname];
      // Build a full TLS options object: shared + identity + origin options.
      sniEntries[hostname] = {
        __proto__: null,
        ...shared,
        ...identity,
        ...(port !== undefined ? { port } : {}),
        ...(authoritative !== undefined ? { authoritative } : {}),
      };
    }

    return {
      __proto__: null,
      ...shared,
      ...defaultIdentity,
      sni: sniEntries,
    };
  }

  // For clients, identity options are specified directly (no sni map).
  // CA and CRL are in the shared options, not per-identity.
  const clientIdentity = processIdentityOptions({
    keys, certs, verifyPrivateKey,
  }, 'options');

  return {
    __proto__: null,
    ...shared,
    ...clientIdentity,
  };
}

/**
 * @param {'use'|'ignore'|'default'} policy
 * @returns {number}
 */
/**
 * Validate and normalize close error options for session.close() and
 * session.destroy(). Returns the options object to pass to C++.
 * @param {object} options
 * @returns {object}
 */
function validateCloseOptions(options) {
  validateObject(options, 'options');
  const {
    code,
    type = 'transport',
    reason,
  } = options;

  if (code !== undefined) {
    if (typeof code !== 'bigint' && typeof code !== 'number') {
      throw new ERR_INVALID_ARG_TYPE('options.code',
                                     ['bigint', 'number'], code);
    }
  }
  validateOneOf(type, 'options.type', ['transport', 'application']);
  if (reason !== undefined) {
    validateString(reason, 'options.reason');
  }

  return { __proto__: null, code, type, reason };
}

function getPreferredAddressPolicy(policy = 'default') {
  switch (policy) {
    case 'use': return kPreferredAddressUse;
    case 'ignore': return kPreferredAddressIgnore;
    case 'default': return kPreferredAddressDefault;
  }
  throw new ERR_INVALID_ARG_VALUE('options.preferredAddressPolicy', policy);
}

/**
 * @param {SessionOptions} options
 * @param {{forServer: boolean, addressFamily: string}} [config]
 * @returns {SessionOptions}
 */
function processSessionOptions(options, config = { __proto__: null }) {
  validateObject(options, 'options');
  const {
    endpoint,
    reuseEndpoint = true,
    version,
    minVersion,
    preferredAddressPolicy = 'default',
    transportParams = kEmptyObject,
    qlog = false,
    sessionTicket,
    token,
    maxPayloadSize,
    unacknowledgedPacketThreshold = 0,
    handshakeTimeout,
    keepAlive,
    maxStreamWindow,
    maxWindow,
    cc,
    datagramDropPolicy = 'drop-oldest',
    drainingPeriodMultiplier = 3,
    maxDatagramSendAttempts = 5,
    // HTTP/3 application-specific options. Nested under `application`
    // to separate protocol-specific settings from transport-level ones.
    application = kEmptyObject,
    // Session callbacks that can be set at construction time to avoid
    // race conditions with events that fire during or immediately
    // after the handshake.
    onerror,
    onstream,
    ondatagram,
    ondatagramstatus,
    onpathvalidation,
    onsessionticket,
    onversionnegotiation,
    onhandshake,
    onnewtoken,
    onearlyrejected,
    onorigin,
    ongoaway,
    onkeylog,
    onqlog,
    // Stream-level callbacks.
    onheaders,
    ontrailers,
    oninfo,
    onwanttrailers,
  } = options;

  const {
    forServer = false,
    targetAddress,
  } = config;

  if (token !== undefined) {
    if (!isArrayBufferView(token)) {
      throw new ERR_INVALID_ARG_TYPE('options.token',
                                     ['ArrayBufferView'], token);
    }
  }

  if (cc !== undefined) {
    validateOneOf(cc, 'options.cc', [CC_ALGO_RENO, CC_ALGO_BBR, CC_ALGO_CUBIC]);
  }

  validateOneOf(datagramDropPolicy, 'options.datagramDropPolicy',
                ['drop-oldest', 'drop-newest']);

  validateInteger(drainingPeriodMultiplier, 'options.drainingPeriodMultiplier',
                  3, 255);

  validateInteger(maxDatagramSendAttempts, 'options.maxDatagramSendAttempts',
                  1, 255);

  // Validate preferred address in transport params if provided.
  const { preferredAddressIpv4, preferredAddressIpv6 } = transportParams;
  if (preferredAddressIpv4 !== undefined) {
    if (!SocketAddress.isSocketAddress(preferredAddressIpv4)) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.transportParams.preferredAddressIpv4',
        'SocketAddress', preferredAddressIpv4);
    }
    if (preferredAddressIpv4.family !== 'ipv4') {
      throw new ERR_INVALID_ARG_VALUE(
        'options.transportParams.preferredAddressIpv4',
        preferredAddressIpv4, 'must be an IPv4 address');
    }
  }
  if (preferredAddressIpv6 !== undefined) {
    if (!SocketAddress.isSocketAddress(preferredAddressIpv6)) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.transportParams.preferredAddressIpv6',
        'SocketAddress', preferredAddressIpv6);
    }
    if (preferredAddressIpv6.family !== 'ipv6') {
      throw new ERR_INVALID_ARG_VALUE(
        'options.transportParams.preferredAddressIpv6',
        preferredAddressIpv6, 'must be an IPv6 address');
    }
  }

  const actualEndpoint = processEndpointOption(endpoint,
                                               reuseEndpoint,
                                               forServer,
                                               targetAddress);

  return {
    __proto__: null,
    endpoint: actualEndpoint,
    version,
    minVersion,
    preferredAddressPolicy: getPreferredAddressPolicy(preferredAddressPolicy),
    transportParams: {
      ...transportParams,
      preferredAddressIpv4: preferredAddressIpv4?.[kSocketAddressHandle],
      preferredAddressIpv6: preferredAddressIpv6?.[kSocketAddressHandle],
    },
    tls: processTlsOptions(options, forServer),
    qlog,
    maxPayloadSize,
    unacknowledgedPacketThreshold,
    handshakeTimeout,
    keepAlive,
    maxStreamWindow,
    maxWindow,
    sessionTicket,
    token,
    cc,
    datagramDropPolicy,
    drainingPeriodMultiplier,
    maxDatagramSendAttempts,
    application,
    onerror,
    onstream,
    ondatagram,
    ondatagramstatus,
    onpathvalidation,
    onsessionticket,
    onversionnegotiation,
    onhandshake,
    onnewtoken,
    onearlyrejected,
    onorigin,
    ongoaway,
    onkeylog,
    onqlog,
    onheaders,
    ontrailers,
    oninfo,
    onwanttrailers,
  };
}

// ============================================================================

/**
 * @param {OnSessionCallback} callback
 * @param {SessionOptions} [options]
 * @returns {Promise<QuicEndpoint>}
 */
async function listen(callback, options = kEmptyObject) {
  validateFunction(callback, 'callback');
  const {
    endpoint,
    ...sessionOptions
  } = processSessionOptions(options, { forServer: true });
  endpoint[kListen](callback, sessionOptions);

  if (onEndpointListeningChannel.hasSubscribers) {
    onEndpointListeningChannel.publish({
      __proto__: null,
      endpoint,
      options,
    });
  }

  return endpoint;
}

/**
 * @param {string|SocketAddress} address
 * @param {SessionOptions} [options]
 * @returns {Promise<QuicSession>}
 */
async function connect(address, options = kEmptyObject) {
  if (typeof address === 'string') {
    address = SocketAddress.parse(address);
  }

  if (!SocketAddress.isSocketAddress(address)) {
    if (address == null || typeof address !== 'object') {
      throw new ERR_INVALID_ARG_TYPE('address', ['SocketAddress', 'string'], address);
    }
    address = new SocketAddress(address);
  }

  const {
    endpoint,
    ...rest
  } = processSessionOptions(options, { targetAddress: address });

  if (onEndpointConnectChannel.hasSubscribers) {
    onEndpointConnectChannel.publish({
      __proto__: null,
      endpoint,
      address,
      options,
    });
  }

  const session = endpoint[kConnect](address[kSocketAddressHandle], rest);

  if (onEndpointClientSessionChannel.hasSubscribers) {
    onEndpointClientSessionChannel.publish({
      __proto__: null,
      endpoint,
      session,
      address,
      options,
    });
  }

  return session;
}

ObjectDefineProperties(QuicEndpoint, {
  Stats: {
    __proto__: null,
    writable: false,
    configurable: false,
    enumerable: true,
    value: QuicEndpointStats,
  },
});
ObjectDefineProperties(QuicSession, {
  Stats: {
    __proto__: null,
    writable: false,
    configurable: false,
    enumerable: true,
    value: QuicSessionStats,
  },
});
ObjectDefineProperties(QuicStream, {
  Stats: {
    __proto__: null,
    writable: false,
    configurable: false,
    enumerable: true,
    value: QuicStreamStats,
  },
});

// ============================================================================

module.exports = {
  listen,
  connect,
  QuicEndpoint,
  QuicError,
  QuicSession,
  QuicStream,
  CC_ALGO_RENO,
  CC_ALGO_CUBIC,
  CC_ALGO_BBR,
  DEFAULT_CIPHERS,
  DEFAULT_GROUPS,
  // These are exported only for internal testing purposes.
  getQuicStreamState,
  getQuicSessionState,
  getQuicEndpointState,
};

/* c8 ignore stop */

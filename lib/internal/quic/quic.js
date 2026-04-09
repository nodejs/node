'use strict';

// TODO(@jasnell) Temporarily ignoring c8 covrerage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  ArrayBufferPrototypeGetByteLength,
  ArrayBufferPrototypeTransfer,
  ArrayIsArray,
  ArrayPrototypePush,
  BigInt,
  DataViewPrototypeGetBuffer,
  DataViewPrototypeGetByteLength,
  DataViewPrototypeGetByteOffset,
  ObjectDefineProperties,
  ObjectKeys,
  PromiseWithResolvers,
  SafeSet,
  SymbolAsyncDispose,
  TypedArrayPrototypeGetBuffer,
  TypedArrayPrototypeGetByteLength,
  TypedArrayPrototypeGetByteOffset,
  TypedArrayPrototypeSlice,
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
  // QUIC_STREAM_HEADERS_KIND_HINTS and QUIC_STREAM_HEADERS_KIND_TRAILING
  // are also available for hints (103) and trailing headers respectively.
  QUIC_STREAM_HEADERS_KIND_INITIAL: kHeadersKindInitial,
  QUIC_STREAM_HEADERS_FLAGS_NONE: kHeadersFlagsNone,
  QUIC_STREAM_HEADERS_FLAGS_TERMINAL: kHeadersFlagsTerminal,
} = internalBinding('quic');

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
  createBlobReaderStream,
  isBlob,
  kHandle: kBlobHandle,
} = require('internal/blob');

const {
  isKeyObject,
} = require('internal/crypto/keys');

const {
  validateBoolean,
  validateFunction,
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
  kBlocked,
  kConnect,
  kDatagram,
  kDatagramStatus,
  kFinishClose,
  kHandshake,
  kHeaders,
  kOwner,
  kRemoveSession,
  kListen,
  kNewSession,
  kRemoveStream,
  kNewStream,
  kNewToken,
  kOnHeaders,
  kOnTrailers,
  kOrigin,
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
const onSessionNewTokenChannel = dc.channel('quic.session.new.token');
const onSessionTicketChannel = dc.channel('quic.session.ticket');
const onSessionVersionNegotiationChannel = dc.channel('quic.session.version.negotiation');
const onSessionOriginChannel = dc.channel('quic.session.receive.origin');
const onSessionHandshakeChannel = dc.channel('quic.session.handshake');

const kNilDatagramId = 0n;

/**
 * @typedef {import('../socketaddress.js').SocketAddress} SocketAddress
 * @typedef {import('../crypto/keys.js').KeyObject} KeyObject
 */

/**
 * @typedef {object} OpenStreamOptions
 * @property {ArrayBuffer|ArrayBufferView|Blob} [body] The outbound payload
 * @property {'high'|'default'|'low'} [priority] The priority level of the stream.
 * @property {boolean} [incremental] Whether to interleave data with same-priority streams.
 */

/**
 * @typedef {object} EndpointOptions
 * @property {string|SocketAddress} [address] The local address to bind to
 * @property {bigint|number} [addressLRUSize] The size of the address LRU cache
 * @property {boolean} [ipv6Only] Use IPv6 only
 * @property {bigint|number} [maxConnectionsPerHost] The maximum number of connections per host
 * @property {bigint|number} [maxConnectionsTotal] The maximum number of total connections
 * @property {bigint|number} [maxRetries] The maximum number of retries
 * @property {bigint|number} [maxStatelessResetsPerHost] The maximum number of stateless resets per host
 * @property {ArrayBufferView} [resetTokenSecret] The reset token secret
 * @property {bigint|number} [retryTokenExpiration] The retry token expiration
 * @property {bigint|number} [tokenExpiration] The token expiration
 * @property {ArrayBufferView} [tokenSecret] The token secret
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
 * @typedef {object} SessionOptions
 * @property {EndpointOptions|QuicEndpoint} [endpoint] An endpoint to use.
 * @property {number} [version] The version
 * @property {number} [minVersion] The minimum version
 * @property {'use'|'ignore'|'default'} [preferredAddressPolicy] The preferred address policy
 * @property {ApplicationOptions} [application] The application options
 * @property {TransportParams} [transportParams] The transport parameters
 * @property {string} [servername] The server name identifier
 * @property {string} [protocol] The application layer protocol negotiation
 * @property {string} [ciphers] The ciphers
 * @property {string} [groups] The groups
 * @property {boolean} [keylog] Enable key logging
 * @property {boolean} [verifyClient] Verify the client
 * @property {boolean} [tlsTrace] Enable TLS tracing
 * @property {boolean} [verifyPrivateKey] Verify the private key
 * @property {KeyObject|KeyObject[]} [keys] The keys
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [certs] The certificates
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [ca] The certificate authority
 * @property {ArrayBuffer|ArrayBufferView|Array<ArrayBuffer|ArrayBufferView>} [crl] The certificate revocation list
 * @property {boolean} [qlog] Enable qlog
 * @property {ArrayBufferView} [sessionTicket] The session ticket
 * @property {bigint|number} [handshakeTimeout] The handshake timeout
 * @property {bigint|number} [keepAlive] The keep-alive timeout in milliseconds. When set,
 *   PING frames will be sent automatically to prevent idle timeout.
 * @property {bigint|number} [maxStreamWindow] The maximum stream window
 * @property {bigint|number} [maxWindow] The maximum window
 * @property {bigint|number} [maxPayloadSize] The maximum payload size
 * @property {bigint|number} [unacknowledgedPacketThreshold] The unacknowledged packet threshold
 * @property {'reno'|'cubic'|'bbr'} [cc] The congestion control algorithm
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
 * @callback OnSessionTicketCallback
 * @this {QuicSession}
 * @param {object} ticket
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
 * @callback OnHeadersCallback
 * @this {QuicStream}
 * @param {object} headers
 * @param {string} kind
 * @returns {void}
 */

/**
 * @callback OnTrailersCallback
 * @this {QuicStream}
 * @returns {void}
 */

/**
 * Provides the callback configuration for the Endpoint|undefined.
 * @typedef {object} EndpointOptions
 * @property {SocketAddress | string} [address] The local address to bind to
 * @property {bigint|number} [retryTokenExpiration] The retry token expiration
 * @property {bigint|number} [tokenExpiration] The token expiration
 * @property {bigint|number} [maxConnectionsPerHost] The maximum number of connections per host
 * @property {bigint|number} [maxConnectionsTotal] The maximum number of total connections
 * @property {bigint|number} [maxStatelessResetsPerHost] The maximum number of stateless resets per host
 * @property {bigint|number} [addressLRUSize] The size of the address LRU cache
 * @property {bigint|number} [maxRetries] The maximum number of retriesw
 * @property {number} [rxDiagnosticLoss] The receive diagnostic loss probability (range 0.0-1.0)
 * @property {number} [txDiagnosticLoss] The transmit diagnostic loss probability (range 0.0-1.0)
 * @property {number} [udpReceiveBufferSize] The UDP receive buffer size
 * @property {number} [udpSendBufferSize] The UDP send buffer size
 * @property {number} [udpTTL] The UDP TTL
 * @property {boolean} [validateAddress] Validate the address
 * @property {boolean} [ipv6Only] Use IPv6 only
 * @property {ArrayBufferView} [resetTokenSecret] The reset token secret
 * @property {ArrayBufferView} [tokenSecret] The token secret
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
    this[kOwner][kPathValidation](result,
                                  new InternalSocketAddress(newLocalAddress),
                                  new InternalSocketAddress(newRemoteAddress),
                                  new InternalSocketAddress(oldLocalAddress),
                                  new InternalSocketAddress(oldRemoteAddress),
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

  onStreamClose(error) {
    // Called when the stream C++ handle has been closed.
    debug(`stream ${this[kOwner].id} closed callback with error: ${error}`);
    this[kOwner][kFinishClose](error);
  },

  onStreamReset(error) {
    // Called when the stream C++ handle has received a stream reset.
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

function validateBody(body) {
  // TODO(@jasnell): Support streaming sources
  if (body === undefined) return body;
  // Transfer ArrayBuffers...
  if (isArrayBuffer(body)) {
    return ArrayBufferPrototypeTransfer(body);
  }
  // With a SharedArrayBuffer, we always copy. We cannot transfer
  // and it's likely unsafe to use the underlying buffer directly.
  if (isSharedArrayBuffer(body)) {
    return new Uint8Array(body).slice();
  }
  if (isArrayBufferView(body)) {
    const size = body.byteLength;
    const offset = body.byteOffset;
    // We have to be careful in this case. If the ArrayBufferView is a
    // subset of the underlying ArrayBuffer, transferring the entire
    // ArrayBuffer could be incorrect if other views are also using it.
    // So if offset > 0 or size != buffer.byteLength, we'll copy the
    // subset into a new ArrayBuffer instead of transferring.
    if (isSharedArrayBuffer(body.buffer) ||
        offset !== 0 || size !== body.buffer.byteLength) {
      return new Uint8Array(body, offset, size).slice();
    }
    // It's still possible that the ArrayBuffer is being used elsewhere,
    // but we really have no way of knowing. We'll just have to trust
    // the caller in this case.
    return new Uint8Array(ArrayBufferPrototypeTransfer(body.buffer), offset, size);
  }
  if (isBlob(body)) return body[kBlobHandle];

  throw new ERR_INVALID_ARG_TYPE('options.body', [
    'ArrayBuffer',
    'ArrayBufferView',
    'Blob',
  ], body);
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
  /** @type {QuicSession} */
  #session;
  /** @type {QuicStreamStats} */
  #stats;
  /** @type {QuicStreamState} */
  #state;
  /** @type {number} */
  #direction = undefined;
  /** @type {OnBlockedCallback|undefined} */
  #onblocked = undefined;
  /** @type {OnStreamErrorCallback|undefined} */
  #onreset = undefined;
  /** @type {OnHeadersCallback|undefined} */
  #onheaders = undefined;
  /** @type {OnTrailersCallback|undefined} */
  #ontrailers = undefined;
  /** @type {Promise<void>} */
  #pendingClose = PromiseWithResolvers();
  #reader;
  /** @type {ReadableStream} */
  #readable;

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

    if (this.pending) {
      debug(`pending ${this.direction} stream created`);
    } else {
      debug(`${this.direction} stream ${this.id} created`);
    }
  }

  /**
   * Returns a ReadableStream to consume incoming data on the stream.
   * @type {ReadableStream<ArrayBufferView>}
   */
  get readable() {
    QuicStream.#assertIsQuicStream(this);
    if (this.#readable === undefined) {
      assert(this.#reader);
      this.#readable = createBlobReaderStream(this.#reader);
    }
    return this.#readable;
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
      this.#onblocked = fn.bind(this);
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
      this.#onreset = fn.bind(this);
      this.#state.wantsReset = true;
    }
  }

  /** @type {OnHeadersCallback} */
  get [kOnHeaders]() {
    return this.#onheaders;
  }

  set [kOnHeaders](fn) {
    if (fn === undefined) {
      this.#onheaders = undefined;
      this.#state[kWantsHeaders] = false;
    } else {
      validateFunction(fn, 'onheaders');
      this.#onheaders = fn.bind(this);
      this.#state[kWantsHeaders] = true;
    }
  }

  /** @type {OnTrailersCallback} */
  get [kOnTrailers]() { return this.#ontrailers; }

  set [kOnTrailers](fn) {
    if (fn === undefined) {
      this.#ontrailers = undefined;
      this.#state[kWantsTrailers] = false;
    } else {
      validateFunction(fn, 'ontrailers');
      this.#ontrailers = fn.bind(this);
      this.#state[kWantsTrailers] = true;
    }
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
   * @returns {boolean}
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
    this.#stats[kFinishClose]();
    this.#state[kFinishClose]();
    this.#session[kRemoveStream](this);
    this.#session = undefined;
    this.#pendingClose.reject = undefined;
    this.#pendingClose.resolve = undefined;
    this.#onblocked = undefined;
    this.#onreset = undefined;
    this.#onheaders = undefined;
    this.#ontrailers = undefined;
    this.#handle = undefined;
  }

  [kBlocked]() {
    // The blocked event should only be called if the stream was created with
    // an onblocked callback. The callback should always exist here.
    assert(this.#onblocked, 'Unexpected stream blocked event');
    this.#onblocked();
  }

  [kReset](error) {
    // The reset event should only be called if the stream was created with
    // an onreset callback. The callback should always exist here.
    assert(this.#onreset, 'Unexpected stream reset event');
    this.#onreset(error);
  }

  [kHeaders](headers, kind) {
    // The headers event should only be called if the stream was created with
    // an onheaders callback. The callback should always exist here.
    assert(this.#onheaders, 'Unexpected stream headers event');
    assert(ArrayIsArray(headers));
    assert(headers.length % 2 === 0);
    const block = {
      __proto__: null,
    };
    for (let n = 0; n + 1 < headers.length; n += 2) {
      if (block[headers[n]] !== undefined) {
        block[headers[n]] = [block[headers[n]], headers[n + 1]];
      } else {
        block[headers[n]] = headers[n + 1];
      }
    }

    this.#onheaders(block, kind);
  }

  [kTrailers]() {
    // The trailers event should only be called if the stream was created with
    // an ontrailers callback. The callback should always exist here.
    assert(this.#ontrailers, 'Unexpected stream trailers event');
    // TODO(@jasnell): Complete the trailers flow. The ontrailers callback
    // should provide a mechanism for the user to send trailing headers
    // using kHeadersKindTrailing. This will be implemented when the
    // QuicSession/QuicStream API model (EventEmitter or Promise-based)
    // is finalized.
    this.#ontrailers();
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `Stream ${inspect({
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
  /** @type {OnStreamCallback} */
  #onstream = undefined;
  /** @type {OnDatagramCallback|undefined} */
  #ondatagram = undefined;
  /** @type {OnDatagramStatusCallback|undefined} */
  #ondatagramstatus = undefined;
  /** @type {{ local: SocketAddress, remote: SocketAddress }|undefined} */
  #path = undefined;
  /** @type {object|undefined} */
  #sessionticket = undefined;
  /** @type {object|undefined} */
  #token = undefined;

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
    this.#stats = new QuicSessionStats(kPrivateConstructor, handle.stats);
    this.#state = new QuicSessionState(kPrivateConstructor, handle.state);
    this.#state.hasVersionNegotiationListener = true;
    this.#state.hasPathValidationListener = true;
    this.#state.hasSessionTicketListener = true;

    debug('session created');
  }

  /** @type {boolean} */
  get #isClosedOrClosing() {
    return this.#handle === undefined || this.#isPendingClose;
  }

  /**
   * Get the session ticket associated with this session, if any.
   * @type {object|undefined}
   */
  get sessionticket() {
    QuicSession.#assertIsQuicSession(this);
    return this.#sessionticket;
  }

  /**
   * Get the NEW_TOKEN token received from the server, if any.
   * This token can be passed as the `token` option on a future
   * connection to the same server to skip address validation.
   * @type {object|undefined}
   */
  get token() {
    QuicSession.#assertIsQuicSession(this);
    return this.#token;
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
      this.#onstream = fn.bind(this);
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
      this.#ondatagram = fn.bind(this);
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
      this.#ondatagramstatus = fn.bind(this);
      this.#state.hasDatagramStatusListener = true;
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
      [kHeaders]: headers,
    } = options;
    if (headers !== undefined) {
      validateObject(headers, 'options.headers');
    }

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

    if (headers !== undefined) {
      if (this.#state.headersSupported === 2) {
        throw new ERR_INVALID_STATE(
          'The negotiated QUIC application protocol does not support headers');
      }
      // If headers are specified and there's no body, then we assume
      // that the headers are terminal.
      handle.sendHeaders(kHeadersKindInitial, buildNgHeaderString(headers),
                         validatedBody === undefined ?
                           kHeadersFlagsTerminal : kHeadersFlagsNone);
    }

    const stream = new QuicStream(kPrivateConstructor, handle, this, direction);
    this.#streams.add(stream);

    if (onSessionOpenStreamChannel.hasSubscribers) {
      onSessionOpenStreamChannel.publish({
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
   * If an ArrayBufferView is given, the underlying ArrayBuffer will be
   * transferred if possible, otherwise the data will be copied.
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
    let offset, length, buffer;

    const maxDatagramSize = this.#state.maxDatagramSize;

    // The peer max datagram size is either unknown or they have explicitly
    // indicated that they do not support datagrams by setting it to 0. In
    // either case, we do not send the datagram.
    if (maxDatagramSize === 0n) return kNilDatagramId;

    if (isPromise(datagram)) {
      datagram = await datagram;
      // Session may have closed while awaiting. Since datagrams are
      // inherently unreliable, silently return rather than throwing.
      if (this.#isClosedOrClosing) return kNilDatagramId;
    }

    if (typeof datagram === 'string') {
      datagram = Buffer.from(datagram, encoding);
      length = TypedArrayPrototypeGetByteLength(datagram);
      if (length === 0) return kNilDatagramId;
    } else {
      if (!isArrayBufferView(datagram)) {
        throw new ERR_INVALID_ARG_TYPE('datagram',
                                       ['ArrayBufferView', 'string'],
                                       datagram);
      }
      if (isDataView(datagram)) {
        offset = DataViewPrototypeGetByteOffset(datagram);
        length = DataViewPrototypeGetByteLength(datagram);
        buffer = DataViewPrototypeGetBuffer(datagram);
      } else {
        offset = TypedArrayPrototypeGetByteOffset(datagram);
        length = TypedArrayPrototypeGetByteLength(datagram);
        buffer = TypedArrayPrototypeGetBuffer(datagram);
      }

      // If the view has zero length (e.g. detached buffer), there's
      // nothing to send.
      if (length === 0) return kNilDatagramId;

      if (isSharedArrayBuffer(buffer) ||
          offset !== 0 ||
          length !== ArrayBufferPrototypeGetByteLength(buffer)) {
        // Copy if the buffer is not transferable (SharedArrayBuffer)
        // or if the view is over a subset of the buffer (e.g. a
        // Node.js Buffer from the pool).
        datagram = TypedArrayPrototypeSlice(
          new Uint8Array(buffer), offset, offset + length);
      } else {
        datagram = new Uint8Array(
          ArrayBufferPrototypeTransfer(buffer), offset, length);
      }
    }

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
    QuicSession.#assertIsQuicSession(this);
    if (!this.#isClosedOrClosing) {
      this.#isPendingClose = true;

      debug('gracefully closing the session');

      this.#handle?.gracefulClose();
      if (onSessionClosingChannel.hasSubscribers) {
        onSessionClosingChannel.publish({
          session: this,
        });
      }
    }
    return this.closed;
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
   */
  destroy(error) {
    QuicSession.#assertIsQuicSession(this);
    if (this.destroyed) return;

    debug('destroying the session');

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
      this.#pendingOpen.reject?.(error);
    } else {
      this.#pendingClose.resolve?.();
    }

    this.#pendingClose.reject = undefined;
    this.#pendingClose.resolve = undefined;
    this.#pendingOpen.reject = undefined;
    this.#pendingOpen.resolve = undefined;

    this.#state[kFinishClose]();
    this.#stats[kFinishClose]();

    this.#onstream = undefined;
    this.#ondatagram = undefined;
    this.#ondatagramstatus = undefined;
    this.#path = undefined;
    this.#sessionticket = undefined;
    this.#token = undefined;

    // Destroy the underlying C++ handle
    this.#handle.destroy();
    this.#handle = undefined;

    if (onSessionClosedChannel.hasSubscribers) {
      onSessionClosedChannel.publish({
        session: this,
        error,
      });
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
    // Otherwise, errorType indicates the type of error that occurred, code indicates
    // the specific error, and reason is an optional string describing the error.
    switch (errorType) {
      case 0: /* Transport Error */
        if (code === 0n) {
          this.destroy();
        } else {
          this.destroy(new ERR_QUIC_TRANSPORT_ERROR(code, reason));
        }
        break;
      case 1: /* Application Error */
        if (code === 0n) {
          this.destroy();
        } else {
          this.destroy(new ERR_QUIC_APPLICATION_ERROR(code, reason));
        }
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
   * @param {Uint8Array} u8 The datagram payload
   * @param {boolean} early A boolean indicating whether this datagram was received before the handshake completed
   */
  [kDatagram](u8, early) {
    // The datagram event should only be called if the session has
    // an ondatagram callback. The callback should always exist here.
    assert(typeof this.#ondatagram === 'function', 'Unexpected datagram event');
    if (this.destroyed) return;
    const length = TypedArrayPrototypeGetByteLength(u8);
    this.#ondatagram(u8, early);

    if (onSessionReceiveDatagramChannel.hasSubscribers) {
      onSessionReceiveDatagramChannel.publish({
        __proto__: null,
        length,
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
    // The datagram status event should only be called if the session has
    // an ondatagramstatus callback. The callback should always exist here.
    assert(typeof this.#ondatagramstatus === 'function', 'Unexpected datagram status event');
    if (this.destroyed) return;
    this.#ondatagramstatus(id, status);

    if (onSessionReceiveDatagramStatusChannel.hasSubscribers) {
      onSessionReceiveDatagramStatusChannel.publish({
        __proto__: null,
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
    if (this.destroyed) return;
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
    if (this.destroyed) return;
    this.#sessionticket = ticket;
    if (onSessionTicketChannel.hasSubscribers) {
      onSessionTicketChannel.publish({
        ticket,
        session: this,
      });
    }
  }

  /**
   * @param {Buffer} token
   * @param {SocketAddress} address
   */
  [kNewToken](token, address) {
    if (this.destroyed) return;
    this.#token = { token, address };
    // TODO(@jasnell): This really should be an event
    if (onSessionNewTokenChannel.hasSubscribers) {
      onSessionNewTokenChannel.publish({
        token,
        address,
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
    if (this.destroyed) return;
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
   * Called when the session receives an ORIGIN frame (RFC 9412).
   * @param {string[]} origins
   */
  [kOrigin](origins) {
    if (this.destroyed) return;
    if (onSessionOriginChannel.hasSubscribers) {
      onSessionOriginChannel.publish({
        origins,
        session: this,
      });
    }
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

    this.#pendingOpen.resolve?.(info);
    this.#pendingOpen.resolve = undefined;
    this.#pendingOpen.reject = undefined;

    if (onSessionHandshakeChannel.hasSubscribers) {
      onSessionHandshakeChannel.publish({
        session: this,
        ...info,
      });
    }
  }

  /**
   * @param {object} handle
   * @param {number} direction
   */
  [kNewStream](handle, direction) {
    const stream = new QuicStream(kPrivateConstructor, handle, this, direction);

    // A new stream was received. If we don't have an onstream callback, then
    // there's nothing we can do about it. Destroy the stream in this case.
    if (typeof this.#onstream !== 'function') {
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

  static {
    getQuicEndpointState = function(endpoint) {
      assertIsQuicEndpoint(endpoint);
      return endpoint.#state;
    };

    assertIsQuicEndpoint = function(val) {
      if (val == null || !(#handle in val)) {
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
      maxConnectionsPerHost,
      maxConnectionsTotal,
      maxStatelessResetsPerHost,
      addressLRUSize,
      maxRetries,
      rxDiagnosticLoss,
      txDiagnosticLoss,
      udpReceiveBufferSize,
      udpSendBufferSize,
      udpTTL,
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
      maxConnectionsPerHost,
      maxConnectionsTotal,
      maxStatelessResetsPerHost,
      addressLRUSize,
      maxRetries,
      rxDiagnosticLoss,
      txDiagnosticLoss,
      udpReceiveBufferSize,
      udpSendBufferSize,
      udpTTL,
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
    return session;
  }

  /**
   * @param {EndpointOptions} config
   */
  constructor(config = kEmptyObject) {
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
    this.#onsession = onsession.bind(this);

    debug('endpoint listening as a server');
    this.#handle.listen(options);
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
    const { sessionTicket, ...rest } = options;

    debug('endpoint connecting as a client');
    const handle = this.#handle.connect(address, rest, sessionTicket);
    if (handle === undefined) {
      throw new ERR_QUIC_CONNECTION_FAILED();
    }
    return this.#newSession(handle);
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
    if (!this.#isClosedOrClosing) {
      this.#pendingError = error;
      // Trigger a graceful close of the endpoint that'll ensure that the
      // endpoint is closed down after all sessions are closed... Because
      // we force all sessions to be abruptly destroyed as the next step,
      // the endpoint will be closed immediately after all the sessions
      // are destroyed.
      this.close();
    }
    // Now, force all sessions to be abruptly closed...
    for (const session of this.#sessions) {
      session.destroy(error);
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
    const maybeCloseError = maybeGetCloseError(context, status, this.#pendingError);
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
    assert(typeof this.#onsession === 'function',
           'onsession callback not specified');
    this.#onsession(session);
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
 * @param {EndpointOptions} endpoint
 * @returns {{ endpoint: Endpoint_, created: boolean }}
 */
function processEndpointOption(endpoint) {
  if (endpoint === undefined) {
    // No endpoint or endpoint options were given. Create a default.
    return new QuicEndpoint();
  } else if (endpoint instanceof QuicEndpoint) {
    // We were given an existing endpoint. Use it as-is.
    return endpoint;
  }
  return new QuicEndpoint(endpoint);
}

/**
 * Validate and extract identity options (keys, certs, ca, crl) from
 * an SNI entry.
 * @param {object} identity
 * @param {string} label
 * @returns {object}
 */
function processIdentityOptions(identity, label) {
  const {
    keys,
    certs,
    ca,
    crl,
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

  if (ca !== undefined) {
    const caInputs = ArrayIsArray(ca) ? ca : [ca];
    for (const caCert of caInputs) {
      if (!isArrayBufferView(caCert) && !isArrayBuffer(caCert)) {
        throw new ERR_INVALID_ARG_TYPE(`${label}.ca`,
                                       ['ArrayBufferView', 'ArrayBuffer'], caCert);
      }
    }
  }

  if (crl !== undefined) {
    const crlInputs = ArrayIsArray(crl) ? crl : [crl];
    for (const crlCert of crlInputs) {
      if (!isArrayBufferView(crlCert) && !isArrayBuffer(crlCert)) {
        throw new ERR_INVALID_ARG_TYPE(`${label}.crl`,
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
    ca,
    crl,
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
  };

  // For servers, identity options come from the sni map.
  // The '*' entry is the default/fallback identity.
  if (forServer) {
    if (sni === undefined || typeof sni !== 'object') {
      throw new ERR_MISSING_ARGS('options.sni');
    }
    if (sni['*'] === undefined) {
      throw new ERR_MISSING_ARGS("options.sni['*']");
    }

    // Process the default ('*') identity into the main tls options.
    const defaultIdentity = processIdentityOptions(sni['*'], "options.sni['*']");
    if (defaultIdentity.keys.length === 0) {
      throw new ERR_MISSING_ARGS("options.sni['*'].keys");
    }
    if (defaultIdentity.certs === undefined) {
      throw new ERR_MISSING_ARGS("options.sni['*'].certs");
    }

    // Build the SNI entries (excluding '*') as full TLS options objects.
    // Each inherits the shared options and overrides the identity fields.
    const sniEntries = { __proto__: null };
    for (const hostname of ObjectKeys(sni)) {
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
  const clientIdentity = processIdentityOptions({
    keys, certs, ca, crl, verifyPrivateKey,
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
  } = options;

  const {
    forServer = false,
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

  const actualEndpoint = processEndpointOption(endpoint);

  return {
    __proto__: null,
    endpoint: actualEndpoint,
    version,
    minVersion,
    preferredAddressPolicy: getPreferredAddressPolicy(preferredAddressPolicy),
    transportParams,
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
  } = processSessionOptions(options);

  const session = endpoint[kConnect](address[kSocketAddressHandle], rest);

  if (onEndpointClientSessionChannel.hasSubscribers) {
    onEndpointClientSessionChannel.publish({
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

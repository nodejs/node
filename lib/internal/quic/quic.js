'use strict';

// TODO(@jasnell) Temporarily ignoring c8 covrerage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  ArrayIsArray,
  ArrayPrototypePush,
  ObjectDefineProperties,
  SafeSet,
  SymbolAsyncDispose,
  SymbolIterator,
  Uint8Array,
} = primordials;

// QUIC requires that Node.js be compiled with crypto support.
const {
  assertCrypto,
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
    ERR_QUIC_CONNECTION_FAILED,
    ERR_QUIC_ENDPOINT_CLOSED,
    ERR_QUIC_OPEN_STREAM_FAILED,
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

const dc = require('diagnostics_channel');
const onEndpointCreatedChannel = dc.channel('quic.endpoint.created');
const onEndpointListeningChannel = dc.channel('quic.endpoint.listen');
const onEndpointClosingChannel = dc.channel('quic.endpoint.closing');
const onEndpointClosedChannel = dc.channel('quic.endpoint.closed');
const onEndpointErrorChannel = dc.channel('quic.endpoint.error');
const onEndpointBusyChangeChannel = dc.channel('quic.endpoint.busy.change');
const onEndpointClientSessionChannel = dc.channel('quic.session.created.client');
const onEndpointServerSessionChannel = dc.channel('quic.session.created.server');

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
 * @property {number} [rxDiagnosticLoss] The receive diagnostic loss  (range 0.0-1.0)
 * @property {number} [txDiagnosticLoss] The transmit diagnostic loss (range 0.0-1.0)
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
 * @param {QuicStream} stream
 * @param {QuicSession} session
 * @returns {void}
 */

/**
 * @callback OnDatagramCallback
 * @param {Uint8Array} datagram
 * @param {QuicSession} session
 * @param {boolean} early
 * @returns {void}
 */

/**
 * @callback OnDatagramStatusCallback
 * @param {bigint} id
 * @param {'lost'|'acknowledged'} status
 * @param {QuicSession} session
 * @returns {void}
 */

/**
 * @callback OnPathValidationCallback
 * @param {'aborted'|'failure'|'success'} result
 * @param {SocketAddress} newLocalAddress
 * @param {SocketAddress} newRemoteAddress
 * @param {SocketAddress} oldLocalAddress
 * @param {SocketAddress} oldRemoteAddress
 * @param {boolean} preferredAddress
 * @param {QuicSession} session
 * @returns {void}
 */

/**
 * @callback OnSessionTicketCallback
 * @param {object} ticket
 * @param {QuicSession} session
 * @returns {void}
 */

/**
 * @callback OnVersionNegotiationCallback
 * @param {number} version
 * @param {number[]} requestedVersions
 * @param {number[]} supportedVersions
 * @param {QuicSession} session
 * @returns {void}
 */

/**
 * @callback OnHandshakeCallback
 * @param {string} sni
 * @param {string} alpn
 * @param {string} cipher
 * @param {string} cipherVersion
 * @param {string} validationErrorReason
 * @param {number} validationErrorCode
 * @param {boolean} earlyDataAccepted
 * @param {QuicSession} session
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
 * @property {OnStreamErrorCallback} onerror The error callback
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
 * @property {OnStreamErrorCallback} onerror The error callback
 * @property {OnBlockedCallback} [onblocked] The blocked callback
 * @property {OnStreamErrorCallback} [onreset] The reset callback
 * @property {OnHeadersCallback} [onheaders] The headers callback
 * @property {OnTrailersCallback} [ontrailers] The trailers callback
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
  onSessionClose(errorType, code, reason) {
    this[kOwner][kFinishClose](errorType, code, reason);
  },
  onSessionDatagram(uint8Array, early) {
    this[kOwner][kDatagram](uint8Array, early);
  },
  onSessionDatagramStatus(id, status) {
    this[kOwner][kDatagramStatus](id, status);
  },
  onSessionHandshake(sni, alpn, cipher, cipherVersion,
                     validationErrorReason,
                     validationErrorCode,
                     earlyDataAccepted) {
    this[kOwner][kHandshake](sni, alpn, cipher, cipherVersion,
                             validationErrorReason, validationErrorCode,
                             earlyDataAccepted);
  },
  onSessionPathValidation(...args) {
    this[kOwner][kPathValidation](...args);
  },
  onSessionTicket(ticket) {
    this[kOwner][kSessionTicket](ticket);
  },
  onSessionVersionNegotiation(version,
                              requestedVersions,
                              supportedVersions) {
    this[kOwner][kVersionNegotiation](version, requestedVersions, supportedVersions);
    // Note that immediately following a version negotiation event, the
    // session will be destroyed.
  },
  // QuicStream callbacks
  onStreamCreated(stream, direction) {
    const session = this[kOwner];
    // The event is ignored if the session has been destroyed.
    if (session.destroyed) {
      stream.destroy();
      return;
    };
    session[kNewStream](stream, direction);
  },
  onStreamBlocked() {
    this[kOwner][kBlocked]();
  },
  onStreamClose(error) {
    this[kOwner][kError](error);
  },
  onStreamReset(error) {
    this[kOwner][kReset](error);
  },
  onStreamHeaders(headers, kind) {
    this[kOwner][kHeaders](headers, kind);
  },
  onStreamTrailers() {
    this[kOwner][kTrailers]();
  },
});

/**
 * @param {'use'|'ignore'|'default'} policy
 * @returns {number}
 */
function getPreferredAddressPolicy(policy = 'default') {
  switch (policy) {
    case 'use': return PREFERRED_ADDRESS_USE;
    case 'ignore': return PREFERRED_ADDRESS_IGNORE;
    case 'default': return DEFAULT_PREFERRED_ADDRESS_POLICY;
  }
  throw new ERR_INVALID_ARG_VALUE('options.preferredAddressPolicy', policy);
}

/**
 * @param {TlsOptions} tls
 */
function processTlsOptions(tls) {
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
  /** @type {OnStreamErrorCallback} */
  #onerror;
  /** @type {OnBlockedCallback|undefined} */
  #onblocked;
  /** @type {OnStreamErrorCallback|undefined} */
  #onreset;
  /** @type {OnHeadersCallback|undefined} */
  #onheaders;
  /** @type {OnTrailersCallback|undefined} */
  #ontrailers;

  /**
   * @param {StreamCallbackConfiguration} config
   * @param {object} handle
   * @param {QuicSession} session
   */
  constructor(config, handle, session, direction) {
    validateObject(config, 'config');
    this.#stats = new QuicStreamStats(kPrivateConstructor, handle.stats);
    this.#state = new QuicStreamState(kPrivateConstructor, handle.stats);
    const {
      onblocked,
      onerror,
      onreset,
      onheaders,
      ontrailers,
    } = config;
    if (onblocked !== undefined) {
      validateFunction(onblocked, 'config.onblocked');
      this.#state.wantsBlock = true;
      this.#onblocked = onblocked;
    }
    if (onreset !== undefined) {
      validateFunction(onreset, 'config.onreset');
      this.#state.wantsReset = true;
      this.#onreset = onreset;
    }
    if (onheaders !== undefined) {
      validateFunction(onheaders, 'config.onheaders');
      this.#state.wantsHeaders = true;
      this.#onheaders = onheaders;
    }
    if (ontrailers !== undefined) {
      validateFunction(ontrailers, 'config.ontrailers');
      this.#state.wantsTrailers = true;
      this.#ontrailers = ontrailers;
    }
    validateFunction(onerror, 'config.onerror');
    this.#onerror = onerror;
    this.#handle = handle;
    this.#session = session;
    this.#direction = direction;
    this.#handle[kOwner] = true;
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

  [kBlocked]() {
    this.#onblocked(this);
  }

  [kError](error) {
    this.#onerror(error, this);
  }

  [kReset](error) {
    this.#onreset(error, this);
  }

  [kHeaders](headers, kind) {
    this.#onheaders(headers, kind, this);
  }

  [kTrailers]() {
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
  /** @type {QuicStream[]} */
  #streams = [];
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

    this.#ondatagram = ondatagram;
    this.#ondatagramstatus = ondatagramstatus;
    this.#onpathvalidation = onpathvalidation;
    this.#onsessionticket = onsessionticket;
    this.#onversionnegotiation = onversionnegotiation;
    this.#onhandshake = onhandshake;
    this.#onstream = onstream;
    this.#endpoint = endpoint;
    this.#streamConfig = stream;

    this.#state.hasDatagramListener = !!ondatagram;
    this.#state.hasPathValidationListener = !!onpathvalidation;
    this.#state.hasSessionTicketListener = !!onsessionticket;
    this.#state.hasVersionNegotiationListener = !!onversionnegotiation;

    this.#handle = handle;
    this.#handle[kOwner] = this;
    this.#stats = new QuicSessionStats(kPrivateConstructor, handle.stats);
    this.#state = new QuicSessionState(kPrivateConstructor, handle.state);
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

  /** @type {Path} */
  get path() {
    if (this.#isClosedOrClosing) return undefined;
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
    const stream = new QuicStream(this.#streamConfig, handle, this, 0);
    ArrayPrototypePush(this.#streams, stream);
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
    const stream = new QuicStream(this.#streamConfig, handle, this, 1);
    ArrayPrototypePush(this.#streams, stream);
    return stream;
  }

  /**
   * Initiate a key update.
   */
  updateKey() {
    if (this.#isClosedOrClosing) {
      throw new ERR_INVALID_STATE('Session is closed');
    }
    this.#handle.updateKey();
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
   */
  destroy(error) {
    // TODO(@jasnell): Implement.
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
      datagram = new Uint8Array(datagram.buffer.transfer(),
                                datagram.byteOffset,
                                datagram.byteLength);
    }
    return this.#handle.sendDatagram(datagram);
  }

  /**
   * @param {Uint8Array} u8
   * @param {boolean} early
   */
  [kDatagram](u8, early) {
    if (this.destroyed) return;
    this.#ondatagram(u8, this, early);
  }

  /**
   * @param {bigint} id
   * @param {'lost'|'acknowledged'} status
   */
  [kDatagramStatus](id, status) {
    if (this.destroyed) return;
    this.#ondatagramstatus(id, status, this);
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
    this.#onpathvalidation(result, newLocalAddress, newRemoteAddress,
                           oldLocalAddress, oldRemoteAddress, preferredAddress,
                           this);
  }

  /**
   * @param {object} ticket
   */
  [kSessionTicket](ticket) {
    if (this.destroyed) return;
    this.#onsessionticket(ticket, this);
  }

  /**
   * @param {number} version
   * @param {number[]} requestedVersions
   * @param {number[]} supportedVersions
   */
  [kVersionNegotiation](version, requestedVersions, supportedVersions) {
    if (this.destroyed) return;
    this.#onversionnegotiation(version, requestedVersions, supportedVersions, this);
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
    this.#onhandshake(sni, alpn, cipher, cipherVersion, validationErrorReason,
                      validationErrorCode, earlyDataAccepted, this);
  }

  /**
   * @param {object} handle
   * @param {number} direction
   */
  [kNewStream](handle, direction) {
    const stream = new QuicStream(this.#streamConfig, handle, this, direction);
    ArrayPrototypePush(this.#streams, stream);
    this.#onstream(stream, this);
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

  [kFinishClose](errorType, code, reason) {
    // TODO(@jasnell): Finish the implementation
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
      onerror,
      onblocked,
      onreset,
      onheaders,
      ontrailers,
    } = config;

    if (onerror !== undefined) {
      validateFunction(onerror, 'config.onerror');
    }
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
      onerror,
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
  }

  /**
   * @param {EndpointCallbackConfiguration} config
   * @param {EndpointOptions} [options]
   */
  constructor(config = kEmptyObject, options = kEmptyObject) {
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

    this.#handle = new Endpoint_(this.#processEndpointOptions(options));
    this.#handle[kOwner] = this;
    this.#stats = new QuicEndpointStats(kPrivateConstructor, this.#handle.stats);
    this.#state = new QuicEndpointState(kPrivateConstructor, this.#handle.state);

    if (onEndpointCreatedChannel.hasSubscribers) {
      onEndpointCreatedChannel.publish({
        endpoint: this,
        options,
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
      preferredAddressPolicy: getPreferredAddressPolicy(preferredAddressPolicy),
      application,
      transportParams,
      tls: processTlsOptions(tls),
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

'use strict';

const {
  DataView,
  MapPrototypeDelete,
  MapPrototypeSet,
  MapPrototypeValues,
  ObjectDefineProperties,
  PromisePrototypeThen,
  PromiseReject,
  PromiseResolve,
  ReflectConstruct,
  SafeMap,
} = primordials;

const {
  createEndpoint: _createEndpoint,
  HTTP3_ALPN,
  IDX_STATE_SESSION_CLIENT_HELLO,  // uint8_t
  IDX_STATE_SESSION_CLIENT_HELLO_DONE,  // uint8_t
  IDX_STATE_SESSION_CLOSING, // uint8_t
  IDX_STATE_SESSION_CLOSING_TIMER_ENABLED, // uint8_t
  IDX_STATE_SESSION_DESTROYED, // uint8_t
  IDX_STATE_SESSION_GRACEFUL_CLOSING, // uint8_t
  IDX_STATE_SESSION_HANDSHAKE_CONFIRMED, // uint8_t
  IDX_STATE_SESSION_IDLE_TIMEOUT, // uint8_t
  IDX_STATE_SESSION_OCSP,  // uint8_t
  IDX_STATE_SESSION_STATELESS_RESET, // uint8_t
  IDX_STATE_SESSION_SILENT_CLOSE, // uint8_t
  IDX_STATE_SESSION_STREAM_OPEN_ALLOWED, // uint8_t
  IDX_STATE_SESSION_USING_PREFERRED_ADDRESS, // uint8_t
  IDX_STATE_SESSION_WRAPPED, // uint8_t
  STREAM_DIRECTION_UNIDIRECTIONAL,
  STREAM_DIRECTION_BIDIRECTIONAL,
  QUICERROR_TYPE_TRANSPORT,
  QUICERROR_TYPE_APPLICATION,
} = internalBinding('quic');

// If the _createEndpoint is undefined, the Node.js binary
// was built without QUIC support, in which case we
// don't want to export anything here.
if (_createEndpoint === undefined)
  return;

const { owner_symbol } = internalBinding('symbols');

const {
  defineEventHandler,
  NodeEventTarget,
} = require('internal/event_target');

const {
  StreamOptions,
  kHandle,
} = require('internal/quic/config');

const {
  createDeferredPromise,
  customInspectSymbol: kInspect,
} = require('internal/util');

const {
  acquireBody,
  createLogStream,
  isPromisePending,
  setPromiseHandled,
  kAddStream,
  kClientHello,
  kClose,
  kCreatedStream,
  kHandshakeComplete,
  kMaybeStreamEvent,
  kOCSP,
  kRemoveSession,
  kRemoveStream,
  kSessionTicket,
  kState,
  kType,
  createStreamEvent,
} = require('internal/quic/common');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');

const {
  Buffer,
} = require('buffer');

const {
  createStream,
  destroyStream,
  setStreamSource,
} = require('internal/quic/stream');

const {
  kHttp3Application,
} = require('internal/quic/http3');

const {
  kEnumerableProperty,
} = require('internal/webstreams/util');

const {
  createSessionStats,
  kDetach: kDetachStats,
  kStats,
} = require('internal/quic/stats');

const {
  InternalX509Certificate,
} = require('internal/crypto/x509');

const {
  InternalSocketAddress,
} = require('internal/socketaddress');

const {
  inspect,
} = require('util');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_INVALID_THIS,
    ERR_QUIC_APPLICATION_ERROR,
    ERR_QUIC_STREAM_OPEN_FAILURE,
    ERR_QUIC_TRANSPORT_ERROR,
  },
} = require('internal/errors');

const assert = require('internal/assert');

/**
 * @typedef {import('./config').StreamOptions} StreamOptions
 * @typedef {import('./config').StreamOptionsInit} StreamOptionsInit
 * @typedef {import('stream').Readable} Readable
 * @typedef {import('../socketaddress').SocketAddress} SocketAddress
 * @typedef {import('../crypto/x509').X509Certificate} X509Certificate
 * @typedef {import('tls').SecureContext} SecureContext
 * @typedef {import('buffer').Blob} Blob
 *
 * @callback ClientHelloCallback
 * @param {SecureContext} [context]
 *
 * @callback OCSPRequestCallback
 * @param {any} [response]
 */

class ClientHello {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  /**
   * @readonly
   * @type {string}
   */
  get alpn() {
    if (!isClientHello(this))
      throw new ERR_INVALID_THIS('ClientHello');
    return this[kState].alpn;
  }

  /**
   * @typedef {{
   *   name: string,
   *   standardName: string,
   *   version: string,
   * }} Cipher
   *
   * @readonly
   * @type {Cipher[]}
   */
  get ciphers() {
    if (!isClientHello(this))
      throw new ERR_INVALID_THIS('ClientHello');
    return this[kState].ciphers;
  }

  /**
   * @readonly
   * @type {string}
   */
  get servername() {
    if (!isClientHello(this))
      throw new ERR_INVALID_THIS('ClientHello');
    return this[kState].servername;
  }

  /**
   * @readonly
   * @type {ClientHelloCallback}
   */
  get done() {
    if (!isClientHello(this))
      throw new ERR_INVALID_THIS('ClientHello');
    return (context) => this[kState].callback(context);
  }

  [kInspect](depth, options) {
    if (!isClientHello(this))
      throw new ERR_INVALID_THIS('ClientHello');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this[kType]} ${inspect({
      alpn: this[kState].alpn,
      ciphers: this[kState].ciphers,
      servername: this[kState].servername,
    }, opts)}`;
  }
}

class OCSPRequest {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  /**
   * @readonly
   * @type {ArrayBuffer}
   */
  get certificate() {
    if (!isOCSPRequest(this))
      throw new ERR_INVALID_THIS('OCSPRequest');
    return this[kState].certificate;
  }

  /**
   * @readonly
   * @type {ArrayBuffer}
   */
  get issuer() {
    if (!isOCSPRequest(this))
      throw new ERR_INVALID_THIS('OCSPRequest');
    return this[kState].issuer;
  }

  /**
   * @readonly
   * @type {OCSPRequestCallback}
   */
  get respondWith() {
    if (!isOCSPRequest(this))
      throw new ERR_INVALID_THIS('OCSPRequest');
    return (response) => this[kState].callback(response);
  }

  [kInspect](depth, options) {
    if (!OCSPRequest(this))
      throw new ERR_INVALID_THIS('OCSPRequest');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this[kType]} ${inspect({
      certificate: this[kState].certificate,
      issuer: this[kState].issuer,
    }, opts)}`;
  }
}

class OCSPResponse {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  /**
   * @readonly
   * @type {any}
   */
  get response() {
    if (!isOCSPResponse(this))
      throw new ERR_INVALID_THIS('OCSPResponse');
    return this[kState].response;
  }

  [kInspect](depth, options) {
    if (!isOCSPResponse(this))
      throw new ERR_INVALID_THIS('OCSPResponse');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this[kType]} ${inspect({
      response: this[kState].response,
    }, opts)}`;
  }
}

const kDefaultApplication = {
  handleHints(stream, hints) {},
  handleRequestHeaders(
    stream,
    headers,
    terminal = false,
    authority = undefined) {},
  handleResponseHeaders(stream, headers, terminal = false) {},
  handleTrailingHeaders(stream, trailers) {},
};

function selectApplication(alpn) {
  switch (alpn) {
    case 'h3':
      return kHttp3Application;
  }
  return kDefaultApplication;
}

ObjectDefineProperties(OCSPRequest.prototype, {
  certificate: kEnumerableProperty,
  issuer: kEnumerableProperty,
  respondWith: kEnumerableProperty,
});

ObjectDefineProperties(OCSPResponse.prototype, {
  response: kEnumerableProperty,
});

ObjectDefineProperties(ClientHello.prototype, {
  alpn: kEnumerableProperty,
  ciphers: kEnumerableProperty,
  servername: kEnumerableProperty,
  done: kEnumerableProperty,
});

class SessionState {
  constructor(state) {
    this[kHandle] = new DataView(state);
    this.wrapped = true;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get ocsp() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_OCSP) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get clientHello() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_CLIENT_HELLO) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get clientHelloDone() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_CLIENT_HELLO_DONE) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get closing() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_CLOSING) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get closingTimerEnabled() {
    return this[kHandle].getUint8(
      IDX_STATE_SESSION_CLOSING_TIMER_ENABLED) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get destroyed() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_DESTROYED) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get gracefulClosing() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_GRACEFUL_CLOSING) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get handshakeConfirmed() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_HANDSHAKE_CONFIRMED) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get idleTimeout() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_IDLE_TIMEOUT) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get statelessReset() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_STATELESS_RESET) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get silentClose() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_SILENT_CLOSE) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get streamOpenAllowed() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_STREAM_OPEN_ALLOWED) === 1;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get usingPreferredAddress() {
    return this[kHandle].getUint8(
      IDX_STATE_SESSION_USING_PREFERRED_ADDRESS) === 1;
  }

  /**
   * @type {boolean}
   */
  get wrapped() {
    return this[kHandle].getUint8(IDX_STATE_SESSION_WRAPPED) === 1;
  }

  /**
   * @type {boolean}
   */
  set wrapped(val) {
    this[kHandle].setUint8(IDX_STATE_SESSION_WRAPPED, val ? 1 : 0);
  }
}

class Session extends NodeEventTarget {
  constructor() { throw new ERR_ILLEGAL_CONSTRUCTOR(); }

  [kClose](errorCode, errorType, silent, statelessReset) {
    // If errorCode is not 0, create an error and destroy
    // Otherwise, just destroy
    this[kState].silentClose = silent;
    this[kState].statelessReset = statelessReset;
    let err;
    if (errorCode) {
      switch (errorType) {
        case QUICERROR_TYPE_APPLICATION:
          err = new ERR_QUIC_APPLICATION_ERROR(errorCode);
          break;
        case QUICERROR_TYPE_TRANSPORT:
          err = new ERR_QUIC_TRANSPORT_ERROR(errorCode);
          break;
      }
    }
    sessionDestroy(this, err);
  }

  [kHandshakeComplete](
    servername,
    alpn,
    ciphername,
    cipherversion,
    maxPacketLength,
    validationErrorReason,
    validationErrorCode,
    earlyData) {
    this[kState].servername = servername;
    this[kState].alpn = alpn;
    this[kState].application = selectApplication(alpn);
    this[kState].cipher = {
      get name() { return ciphername; },
      get version() { return cipherversion; },
    };
    this[kState].maxPacketLength = maxPacketLength;
    if (validationErrorReason !== undefined) {
      this[kState].validationError = {
        get reason() { return validationErrorReason; },
        get code() { return validationErrorCode; },
      };
    }
    this[kState].earlyData = earlyData;
    this[kState].handshake.resolve();
  }

  [kCreatedStream](handle) {
    const stream = createStream(handle, this);
    this[kAddStream](stream);
    this[kMaybeStreamEvent](stream);
  }

  [kMaybeStreamEvent](stream) {
    if (this[kState].alpn === HTTP3_ALPN &&
        isPromisePending(stream[kState].headers.promise)) {
      return;
    }
    PromisePrototypeThen(
      PromiseResolve(),
      () => this.dispatchEvent(createStreamEvent(stream)),
      (error) => destroyStream(stream, error));
  }

  [kAddStream](stream) {
    MapPrototypeSet(this[kState].streams, stream.id, stream);
  }

  [kRemoveStream](stream) {
    MapPrototypeDelete(this[kState].streams, stream.id);
  }

  [kClientHello](alpn, servername, ciphers, callback) {
    assert(this[kState].clienthello?.resolve !== undefined);
    this[kState].clienthello.resolve(
      createClientHello(
        alpn,
        servername,
        ciphers,
        callback));
  }

  [kOCSP](type, options) {
    assert(this[kState].ocsp?.resolve !== undefined);
    let value;
    switch (type) {
      case 'request':
        const {
          certificate,
          issuer,
          callback,
        } = options;
        value = createOCSPRequest(certificate, issuer, callback);
        break;
      case 'response':
        const {
          response,
        } = options;
        value = createOCSPResponse(response);
        break;
    }
    this[kState].ocsp.resolve(value);
  }

  [kSessionTicket](sessionTicket, transportParams) {
    this[kState].sessionTicket = { sessionTicket, transportParams };
  }

  /**
   * @param {string | ArrayBuffer | TypedArray | DataView} data
   * @param {string} [encoding]
   * @returns {boolean}
   */
  datagram(data, encoding) {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');

    if (isSessionDestroyed(this))
      throw new ERR_INVALID_STATE('Session is already destroyed');
    if (this[kState].closeRequested)
      throw new ERR_INVALID_STATE('Session is closing');
    // if (!this[kState].inner.streamOpenAllowed)
    //   throw new ERR_INVALID_STATE('Opening streams is not yet allowed');

    if (typeof data === 'string')
      data = Buffer.from(data, encoding);
    if (!isAnyArrayBuffer(data) && !isArrayBufferView(data)) {
      throw new ERR_INVALID_ARG_TYPE(
        'data',
        [
          'string',
          'ArrayBuffer',
          'Buffer',
          'TypedArray',
          'DataView',
        ],
        data);
    }

    if (data.byteLength === 0)
      return false;

    return this[kHandle].sendDatagram(data);
  }

  /**
   * @param {StreamOptionsInit | StreamOptions} options
   */
  open(options = new StreamOptions()) {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');

    if (isSessionDestroyed(this))
      throw new ERR_INVALID_STATE('Session is already destroyed');
    if (this[kState].closeRequested)
      throw new ERR_INVALID_STATE('Session is closing');
    if (!this[kState].inner.streamOpenAllowed)
      throw new ERR_INVALID_STATE('Opening streams is not yet allowed');

    if (!StreamOptions.isStreamOptions(options)) {
      if (options === null || typeof options !== 'object') {
        throw new ERR_INVALID_ARG_TYPE('options', [
          'StreamOptions',
          'Object',
        ], options);
      }
      options = new StreamOptions(options);
    }

    const {
      unidirectional,
      headers,
      trailers,
      body,
    } = options;

    const handle = this[kHandle].openStream(
      unidirectional ?
        STREAM_DIRECTION_UNIDIRECTIONAL :
        STREAM_DIRECTION_BIDIRECTIONAL
    );
    if (handle === undefined)
      throw new ERR_QUIC_STREAM_OPEN_FAILURE();

    const stream = createStream(handle, this, trailers);
    this[kAddStream](stream);
    PromisePrototypeThen(
      acquireBody(body, stream),
      async (actual) => {
        await this[kState].application.handleRequestHeaders(
          handle,
          headers,
          actual === undefined,
          this[kState].authority);
        setStreamSource(stream, actual);
      },
      (error) => stream.destroy(error));

    return stream;
  }

  /**
   * Initiates a key update for this session.
   * @returns {void}
   */
  updateKey() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    if (isSessionDestroyed(this))
      throw new ERR_INVALID_STATE('Session is already destroyed');
    if (this[kState].closeRequested)
      throw new ERR_INVALID_STATE('Session is closing');

    this[kHandle].updateKey();
  }

  /**
   * @readonly
   * @type {Promise<ClientHello>}
   */
  get clienthello() {
    if (!isSession(this))
      return PromiseReject(new ERR_INVALID_THIS('Session'));
    return this[kState].clienthello?.promise;
  }

  /**
   * @readonly
   * @type {Promise<OCSPRequest | OCSPResponse>}
   */
  get ocsp() {
    if (!isSession(this))
      return PromiseReject(new ERR_INVALID_THIS('Session'));
    return this[kState].ocsp?.promise;
  }

  /**
   * @readonly
   * @type {Promise<any>}
   */
  get closed() {
    if (!isSession(this))
      PromiseReject(new ERR_INVALID_THIS('Session'));
    return this[kState].closed.promise;
  }

  /**
   * Initiates a graceful shutdown of the Session. Existing streams are allowed
   * to complete normally. Calls to openStream() will throw and new
   * peer-initiated streams are rejected immediately. Returns a promise that
   * is successfully fulfilled once the Session is destroyed, or rejected if
   * an error occurs at any time while close is pending.
   * @returns {Promise<void>}
   */
  close() {
    if (!isSession(this))
      PromiseReject(new ERR_INVALID_THIS('Session'));
    if (isSessionDestroyed(this)) {
      return PromiseReject(
        new ERR_INVALID_STATE('Session is already destroyed'));
    }

    return sessionClose(this);
  }

  /**
   * @param {any} [reason]
   */
  cancel(reason) {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    if (isSessionDestroyed(this))
      return;

    sessionDestroy(this, reason);
  }

  /**
   * @readonly
   * @type {string | void}
   */
  get alpn() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return this[kState].alpn;
  }

  /**
   * @readonly
   * @type {X509Certificate | void}
   */
  get certificate() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return sessionCertificate(this);
  }

  /**
   * @readonly
   * @type {{
   *   name: string,
   *   version: string
   * } | void}
   */
  get cipher() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return this[kState].cipher;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get closing() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return this[kState].closeRequested;
  }

  /**
   * @readonly
   * @type {boolean}
   */
  get earlyData() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return this[kState].earlyData;
  }

  /**
   * @readonly
   * @type {Promise<void>}
   */
  get handshake() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return this[kState].handshake.promise;
  }

  /**
   * @readonly
   * @type {{
   *   name?: string,
   *   size: number,
   *   type: string,
   * } | void}
   */
  get ephemeralKeyInfo() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return sessionEphemeralKeyInfo(this);
  }

  /**
   * @readonly
   * @type {X509Certificate | void}
   */
  get peerCertificate() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return sessionPeerCertificate(this);
  }

  /**
   * @readonly
   * @type {Readable}
   */
  get qlog() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');

    if (this[kState].qlog === undefined && this[kHandle].qlog)
      this[kState].qlog = createLogStream(this[kHandle].qlog);

    return this[kState].qlog;
  }

  /**
   * @readonly
   * @type {Readable}
   */
  get keylog() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');

    if (this[kState].keylog === undefined && this[kHandle].keylog)
      this[kState].keylog = createLogStream(this[kHandle].keylog);

    return this[kState].keylog;
  }

  /**
   * @readonly
   * @type {SocketAddress | void}
   */
  get remoteAddress() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return sessionRemoteAddress(this);
  }

  /**
   * @readonly
   * @type {string | void}
   */
  get servername() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return this[kState].servername;
  }

  /**
   * @readonly
   * @type {Blob}
   */
  get sessionTicket() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return this[kState].sessionTicket;
  }

  /**
   * @readonly
   * @type {{
   *   reason: string,
   *   code: string
   * } | void}
   */
  get validationError() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return this[kState].validationError;
  }

  /**
   * @readonly
   * @type {import('./stats').SessionStats}
   */
  get stats() {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    return this[kStats];
  }

  [kInspect](depth, options) {
    if (!isSession(this))
      throw new ERR_INVALID_THIS('Session');
    if (depth < 0)
      return this;

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `${this[kType]} ${inspect({
      alpn: this[kState].alpn,
      certificate: sessionCertificate(this),
      closed: this[kState].closed.promise,
      closing: this[kState].closeRequested,
      earlyData: this[kState].earlyData,
      ephemeralKeyInfo: sessionEphemeralKeyInfo(this),
      handshake: this[kState].handshake.promise,
      handshakeConfirmed: this[kState].inner?.handshakeConfirmed,
      idleTimeout: this[kState].inner?.idleTimeout,
      peerCertificate: sessionPeerCertificate(this),
      remoteAddress: sessionRemoteAddress(this),
      servername: this[kState].servername,
      silentClose: this[kState].silentClose,
      statelessReset: this[kState].statelessReset,
      stats: this[kStats],
      usingPreferredAddress: this[kState].inner?.usingPreferredAddress,
    }, opts)}`;
  }
}

ObjectDefineProperties(Session.prototype, {
  alpn: kEnumerableProperty,
  cancel: kEnumerableProperty,
  certificate: kEnumerableProperty,
  cipher: kEnumerableProperty,
  clienthello: kEnumerableProperty,
  close: kEnumerableProperty,
  closed: kEnumerableProperty,
  closing: kEnumerableProperty,
  earlyData: kEnumerableProperty,
  ephemeralKeyInfo: kEnumerableProperty,
  handshake: kEnumerableProperty,
  keylog: kEnumerableProperty,
  ocsp: kEnumerableProperty,
  open: kEnumerableProperty,
  peerCertificate: kEnumerableProperty,
  qlog: kEnumerableProperty,
  remoteAddress: kEnumerableProperty,
  servername: kEnumerableProperty,
  stats: kEnumerableProperty,
  updateKey: kEnumerableProperty,
  validationError: kEnumerableProperty,
});

defineEventHandler(Session.prototype, 'stream');
defineEventHandler(Session.prototype, 'datagram');

function isOCSPRequest(value) {
  return typeof value?.[kState] === 'object' &&
         value?.[kType] === 'OCSPRequest';
}

function isOCSPResponse(value) {
  return typeof value?.[kState] === 'object' &&
         value?.[kType] === 'OCSPResponse';
}

function isClientHello(value) {
  return typeof value?.[kState] === 'object' &&
         value?.[kType] === 'ClientHello';
}

function isSession(value) {
  return typeof value?.[kState] === 'object' && value?.[kType] === 'Session';
}

function createOCSPRequest(certificate, issuer, callback) {
  return ReflectConstruct(
    function() {
      this[kType] = 'OCSPRequest';
      this[kState] = {
        certificate,
        issuer,
        callback,
      };
    },
    [],
    OCSPRequest);
}

function createOCSPResponse(response) {
  return ReflectConstruct(
    function() {
      this[kType] = 'OCSPResponse';
      this[kState] = { response };
    },
    [],
    OCSPResponse);
}

/**
 * @param {string} alpn
 * @param {string} servername
 * @param {string[]} ciphers
 * @param {ClientHelloCallback} callback
 * @returns {void}
 */
function createClientHello(alpn, servername, ciphers, callback) {
  return ReflectConstruct(
    function() {
      this[kType] = 'ClientHello';
      this[kState] = {
        alpn,
        servername,
        ciphers,
        callback,
      };
    },
    [],
    ClientHello);
}

/**
 * @typedef {import('../abort_controller').AbortSignal} AbortSignal
 * @param {EndpointWrap} handle
 * @returns {Session}
 */
function createSession(
  handle,
  endpoint,
  alpn = undefined,
  authority = undefined) {
  const ret = ReflectConstruct(
    class extends NodeEventTarget {
      constructor() {
        super();
        this[kType] = 'Session';
        const inner = new SessionState(handle.state);
        this[kState] = {
          alpn,
          application: selectApplication(alpn),
          authority,
          certificate: undefined,
          closeRequested: false,
          closed: createDeferredPromise(),
          cipher: undefined,
          clienthello: undefined,
          earlyData: false,
          endpoint,
          handshake: createDeferredPromise(),
          inner,
          keylog: undefined,
          maxPacketLength: 0,
          ocsp: undefined,
          peerCertificate: undefined,
          qlog: undefined,
          remoteAddress: undefined,
          servername: undefined,
          sessionTicket: undefined,
          silentClose: false,
          statelessReset: false,
          streams: new SafeMap(),
          validationError: undefined,
        };
        this[kHandle] = handle;
        this[kStats] = createSessionStats(handle.stats);
        setPromiseHandled(this[kState].closed.promise);
        setPromiseHandled(this[kState].handshake.promise);

        if (inner.clientHello) {
          this[kState].clienthello = createDeferredPromise();
          setPromiseHandled(this[kState].clienthello.promise);
        }

        if (inner.ocsp) {
          this[kState].ocsp = createDeferredPromise();
          setPromiseHandled(this[kState].ocsp.promise);
        }
      }
    },
    [],
    Session);
  ret[kHandle][owner_symbol] = ret;
  return ret;
}

function isSessionDestroyed(session) {
  return session[kHandle] === undefined;
}

function sessionRemoteAddress(session) {
  if (session[kState].remoteAddress === undefined) {
    const ret = session[kHandle]?.getRemoteAddress();
    session[kState].remoteAddress =
      ret === undefined ? undefined : new InternalSocketAddress(ret);
  }
  return session[kState].remoteAddress;
}

function sessionPeerCertificate(session) {
  if (session[kState].peerCertificate === undefined) {
    const ret = session[kHandle]?.getPeerCertificate();
    session[kState].peerCertificate =
      ret === undefined ? undefined : new InternalX509Certificate(ret);
  }
  return session[kState].peerCertificate;
}

function sessionCertificate(session) {
  if (session[kState].certificate === undefined) {
    const ret = session[kHandle]?.getCertificate();
    session[kState].certificate =
      ret === undefined ? undefined : new InternalX509Certificate(ret);
  }
  return session[kState].certificate;
}

function sessionEphemeralKeyInfo(session) {
  return session[kHandle]?.getEphemeralKeyInfo();
}

function sessionDestroy(session, error) {
  if (isSessionDestroyed(session))
    return;
  const state = session[kState];

  for (const stream of MapPrototypeValues(state.streams))
    destroyStream(stream, error);

  const handle = session[kHandle];
  session[kHandle][owner_symbol] = undefined;
  session[kHandle] = undefined;

  state.inner = undefined;
  session[kStats][kDetachStats]();

  session[kState].endpoint[kRemoveSession](this);
  session[kState].endpoint = undefined;

  handle.destroy();

  if (error) {
    if (isPromisePending(session[kState].handshake.promise))
      session[kState].handshake.reject(error);
    state.closed.reject(error);
    return;
  }

  if (isPromisePending(session[kState].handshake.promise))
    session[kState].handshake.resolve();
  state.closed.resolve();
}

function sessionClose(session) {
  const state = session[kState];

  if (!state.closeRequested) {
    state.closeRequested = true;
    session[kHandle].gracefulClose();
  }

  return state.closed.promise;
}

module.exports = {
  ClientHello,
  OCSPRequest,
  OCSPResponse,
  Session,
  createSession,
  isSession,
  sessionDestroy,
};

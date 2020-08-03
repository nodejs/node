'use strict';

/* eslint-disable no-use-before-define */

const {
  assertCrypto,
  customInspectSymbol: kInspect,
} = require('internal/util');

assertCrypto();

const {
  Array,
  BigInt64Array,
  Boolean,
  Error,
  Map,
  Number,
  Promise,
  PromiseAll,
  PromiseReject,
  PromiseResolve,
  Set,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');
const { isArrayBufferView } = require('internal/util/types');
const {
  customInspect,
  getAllowUnauthorized,
  getSocketType,
  setTransportParams,
  toggleListeners,
  validateNumber,
  validateTransportParams,
  validateQuicClientSessionOptions,
  validateQuicSocketOptions,
  validateQuicStreamOptions,
  validateQuicSocketListenOptions,
  validateQuicEndpointOptions,
  validateCreateSecureContextOptions,
  validateQuicSocketConnectOptions,
  QuicStreamSharedState,
  QuicSocketSharedState,
  QuicSessionSharedState,
  QLogStream,
} = require('internal/quic/util');
const assert = require('internal/assert');
const { EventEmitter, once } = require('events');
const fs = require('fs');
const fsPromisesInternal = require('internal/fs/promises');
const { Duplex } = require('stream');
const {
  createSecureContext: _createSecureContext
} = require('tls');
const {
  translatePeerCertificate
} = require('_tls_common');
const {
  defaultTriggerAsyncIdScope,
  symbols: {
    async_id_symbol,
    owner_symbol,
  },
} = require('internal/async_hooks');
const dgram = require('dgram');
const internalDgram = require('internal/dgram');
const {
  assertValidPseudoHeader,
  assertValidPseudoHeaderResponse,
  assertValidPseudoHeaderTrailer,
  mapToHeaders,
} = require('internal/http2/util');

const {
  constants: {
    UV_UDP_IPV6ONLY,
    UV_UDP_REUSEADDR,
  }
} = internalBinding('udp_wrap');

const {
  writeGeneric,
  writevGeneric,
  onStreamRead,
  kAfterAsyncWrite,
  kMaybeDestroy,
  kUpdateTimer,
  kHandle,
  setStreamTimeout // eslint-disable-line no-unused-vars
} = require('internal/stream_base_commons');

const {
  ShutdownWrap,
  kReadBytesOrError,
  streamBaseState
} = internalBinding('stream_wrap');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_OPERATION_FAILED,
    ERR_QUIC_FAILED_TO_CREATE_SESSION,
    ERR_QUIC_INVALID_REMOTE_TRANSPORT_PARAMS,
    ERR_QUIC_INVALID_TLS_SESSION_TICKET,
    ERR_QUIC_VERSION_NEGOTIATION,
    ERR_TLS_DH_PARAM_SIZE,
  },
  hideStackFrames,
  errnoException,
  exceptionWithHostPort
} = require('internal/errors');

const { FileHandle } = internalBinding('fs');
const { StreamPipe } = internalBinding('stream_pipe');
const { UV_EOF } = internalBinding('uv');

const {
  QuicSocket: QuicSocketHandle,
  QuicEndpoint: QuicEndpointHandle,
  initSecureContext,
  initSecureContextClient,
  createClientSession: _createClientSession,
  openBidirectionalStream: _openBidirectionalStream,
  openUnidirectionalStream: _openUnidirectionalStream,
  setCallbacks,
  constants: {
    AF_INET6,
    NGTCP2_DEFAULT_MAX_PKTLEN,
    IDX_QUIC_SESSION_STATS_CREATED_AT,
    IDX_QUIC_SESSION_STATS_DESTROYED_AT,
    IDX_QUIC_SESSION_STATS_HANDSHAKE_START_AT,
    IDX_QUIC_SESSION_STATS_BYTES_RECEIVED,
    IDX_QUIC_SESSION_STATS_BYTES_SENT,
    IDX_QUIC_SESSION_STATS_BIDI_STREAM_COUNT,
    IDX_QUIC_SESSION_STATS_UNI_STREAM_COUNT,
    IDX_QUIC_SESSION_STATS_STREAMS_IN_COUNT,
    IDX_QUIC_SESSION_STATS_STREAMS_OUT_COUNT,
    IDX_QUIC_SESSION_STATS_KEYUPDATE_COUNT,
    IDX_QUIC_SESSION_STATS_LOSS_RETRANSMIT_COUNT,
    IDX_QUIC_SESSION_STATS_HANDSHAKE_COMPLETED_AT,
    IDX_QUIC_SESSION_STATS_ACK_DELAY_RETRANSMIT_COUNT,
    IDX_QUIC_SESSION_STATS_MAX_BYTES_IN_FLIGHT,
    IDX_QUIC_SESSION_STATS_BLOCK_COUNT,
    IDX_QUIC_SESSION_STATS_MIN_RTT,
    IDX_QUIC_SESSION_STATS_SMOOTHED_RTT,
    IDX_QUIC_SESSION_STATS_LATEST_RTT,
    IDX_QUIC_STREAM_STATS_CREATED_AT,
    IDX_QUIC_STREAM_STATS_DESTROYED_AT,
    IDX_QUIC_STREAM_STATS_BYTES_RECEIVED,
    IDX_QUIC_STREAM_STATS_BYTES_SENT,
    IDX_QUIC_STREAM_STATS_MAX_OFFSET,
    IDX_QUIC_STREAM_STATS_FINAL_SIZE,
    IDX_QUIC_STREAM_STATS_MAX_OFFSET_ACK,
    IDX_QUIC_STREAM_STATS_MAX_OFFSET_RECV,
    IDX_QUIC_SOCKET_STATS_CREATED_AT,
    IDX_QUIC_SOCKET_STATS_DESTROYED_AT,
    IDX_QUIC_SOCKET_STATS_BOUND_AT,
    IDX_QUIC_SOCKET_STATS_LISTEN_AT,
    IDX_QUIC_SOCKET_STATS_BYTES_RECEIVED,
    IDX_QUIC_SOCKET_STATS_BYTES_SENT,
    IDX_QUIC_SOCKET_STATS_PACKETS_RECEIVED,
    IDX_QUIC_SOCKET_STATS_PACKETS_IGNORED,
    IDX_QUIC_SOCKET_STATS_PACKETS_SENT,
    IDX_QUIC_SOCKET_STATS_SERVER_SESSIONS,
    IDX_QUIC_SOCKET_STATS_CLIENT_SESSIONS,
    IDX_QUIC_SOCKET_STATS_STATELESS_RESET_COUNT,
    IDX_QUIC_SOCKET_STATS_SERVER_BUSY_COUNT,
    ERR_FAILED_TO_CREATE_SESSION,
    ERR_INVALID_REMOTE_TRANSPORT_PARAMS,
    ERR_INVALID_TLS_SESSION_TICKET,
    NGTCP2_PATH_VALIDATION_RESULT_FAILURE,
    NGTCP2_NO_ERROR,
    QUIC_ERROR_APPLICATION,
    QUICSERVERSESSION_OPTION_REJECT_UNAUTHORIZED,
    QUICSERVERSESSION_OPTION_REQUEST_CERT,
    QUICCLIENTSESSION_OPTION_REQUEST_OCSP,
    QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY,
    QUICSOCKET_OPTIONS_VALIDATE_ADDRESS,
    QUICSTREAM_HEADERS_KIND_NONE,
    QUICSTREAM_HEADERS_KIND_INFORMATIONAL,
    QUICSTREAM_HEADERS_KIND_INITIAL,
    QUICSTREAM_HEADERS_KIND_TRAILING,
    QUICSTREAM_HEADERS_KIND_PUSH,
    QUICSTREAM_HEADER_FLAGS_NONE,
    QUICSTREAM_HEADER_FLAGS_TERMINAL,
  }
} = internalBinding('quic');

const {
  Histogram,
  kDestroy: kDestroyHistogram
} = require('internal/histogram');

const {
  validateBoolean,
  validateInteger,
  validateObject,
} = require('internal/validators');

const emit = EventEmitter.prototype.emit;

const kAddSession = Symbol('kAddSession');
const kAddStream = Symbol('kAddStream');
const kBind = Symbol('kBind');
const kClose = Symbol('kClose');
const kClientHello = Symbol('kClientHello');
const kDestroy = Symbol('kDestroy');
const kEndpointBound = Symbol('kEndpointBound');
const kEndpointClose = Symbol('kEndpointClose');
const kHandleOcsp = Symbol('kHandleOcsp');
const kHandshake = Symbol('kHandshake');
const kHandshakeComplete = Symbol('kHandshakeComplete');
const kHandshakePost = Symbol('kHandshakePost');
const kHeaders = Symbol('kHeaders');
const kInternalState = Symbol('kInternalState');
const kInternalClientState = Symbol('kInternalClientState');
const kInternalServerState = Symbol('kInternalServerState');
const kListen = Symbol('kListen');
const kMaybeBind = Symbol('kMaybeBind');
const kOnFileOpened = Symbol('kOnFileOpened');
const kOnFileUnpipe = Symbol('kOnFileUnpipe');
const kOnPipedFileHandleRead = Symbol('kOnPipedFileHandleRead');
const kReady = Symbol('kReady');
const kRemoveFromSocket = Symbol('kRemoveFromSocket');
const kRemoveSession = Symbol('kRemove');
const kRemoveStream = Symbol('kRemoveStream');
const kServerBusy = Symbol('kServerBusy');
const kSetHandle = Symbol('kSetHandle');
const kSetQLogStream = Symbol('kSetQLogStream');
const kSetSocket = Symbol('kSetSocket');
const kStartFilePipe = Symbol('kStartFilePipe');
const kStreamClose = Symbol('kStreamClose');
const kStreamOptions = Symbol('kStreamOptions');
const kStreamReset = Symbol('kStreamReset');
const kTrackWriteState = Symbol('kTrackWriteState');
const kUDPHandleForTesting = Symbol('kUDPHandleForTesting');
const kUsePreferredAddress = Symbol('kUsePreferredAddress');
const kVersionNegotiation = Symbol('kVersionNegotiation');
const kWriteGeneric = Symbol('kWriteGeneric');

const kRejections = Symbol.for('nodejs.rejection');

const kSocketUnbound = 0;
const kSocketPending = 1;
const kSocketBound = 2;
const kSocketDestroyed = 3;

let diagnosticPacketLossWarned = false;
let warnedVerifyHostnameIdentity = false;

let DOMException;

const lazyDOMException = hideStackFrames((message) => {
  if (DOMException === undefined)
    DOMException = internalBinding('messaging').DOMException;
  return new DOMException(message);
});

assert(process.versions.ngtcp2 !== undefined);

// Called by the C++ internals when the QuicSocket is closed with
// or without an error. The only thing left to do is destroy the
// QuicSocket instance.
function onSocketClose(err) {
  this[owner_symbol].destroy(err != null ? errnoException(err) : undefined);
}

// Called by the C++ internals when the server busy state of
// the QuicSocket has been changed.
function onSocketServerBusy() {
  this[owner_symbol][kServerBusy]();
}

// Called by the C++ internals when a new server QuicSession has been created.
function onSessionReady(handle) {
  const socket = this[owner_symbol];
  const session =
    new QuicServerSession(
      socket,
      handle,
      socket[kStreamOptions]);
  try {
    socket.emit('session', session);
  } catch (error) {
    socket[kRejections](error, 'session', session);
  }
}

// Called when the C++ QuicSession::Close() method has been called.
// Synchronously cleanup and destroy the JavaScript QuicSession.
function onSessionClose(code, family, silent, statelessReset) {
  this[owner_symbol][kDestroy](code, family, silent, statelessReset);
}

// This callback is invoked at the start of the TLS handshake to provide
// some basic information about the ALPN, SNI, and Ciphers that are
// being requested. It is only called if the 'clientHelloHandler' option is
// specified on listen().
function onSessionClientHello(alpn, servername, ciphers) {
  this[owner_symbol][kClientHello](alpn, servername, ciphers)
    .then((context) => {
      if (context !== undefined && !context?.context)
        throw new ERR_INVALID_ARG_TYPE('context', 'SecureContext', context);
      this.onClientHelloDone(context?.context);
    })
    .catch((error) => this[owner_symbol].destroy(error));
}

// This callback is only ever invoked for QuicServerSession instances,
// and is used to trigger OCSP request processing when needed. The
// user callback must invoke .onCertDone() in order for the
// TLS handshake to continue.
function onSessionCert(servername) {
  this[owner_symbol][kHandleOcsp](servername)
    .then((data) => {
      if (data !== undefined) {
        if (typeof data === 'string')
          data = Buffer.from(data);
        if (!isArrayBufferView(data)) {
          throw new ERR_INVALID_ARG_TYPE(
            'data',
            ['string', 'Buffer', 'TypedArray', 'DataView'],
            data);
        }
      }
      this.onCertDone(data);
    })
    .catch((error) => this[owner_symbol].destroy(error));
}

// This callback is only ever invoked for QuicClientSession instances,
// and is used to deliver the OCSP response as provided by the server.
// If the requestOCSP configuration option is false, this will never
// be called.
function onSessionStatus(data) {
  this[owner_symbol][kHandleOcsp](data)
    .catch((error) => this[owner_symbol].destroy(error));
}

// Called by the C++ internals when the TLS handshake is completed.
function onSessionHandshake(
  servername,
  alpn,
  cipher,
  cipherVersion,
  maxPacketLength,
  verifyErrorReason,
  verifyErrorCode,
  earlyData) {
  this[owner_symbol][kHandshake](
    servername,
    alpn,
    cipher,
    cipherVersion,
    maxPacketLength,
    verifyErrorReason,
    verifyErrorCode,
    earlyData);
}

// Called by the C++ internals when TLS session ticket data is
// available. This is generally most useful on the client side
// where the session ticket needs to be persisted for session
// resumption and 0RTT.
function onSessionTicket(sessionTicket, transportParams) {
  if (this[owner_symbol]) {
    process.nextTick(
      emit.bind(
        this[owner_symbol],
        'sessionTicket',
        sessionTicket,
        transportParams));
  }
}

// Called by the C++ internals when path validation is completed.
// This is a purely informational event that is emitted only when
// there is a listener present for the pathValidation event.
function onSessionPathValidation(res, local, remote) {
  const session = this[owner_symbol];
  if (session) {
    process.nextTick(
      emit.bind(
        session,
        'pathValidation',
        res === NGTCP2_PATH_VALIDATION_RESULT_FAILURE ? 'failure' : 'success',
        local,
        remote));
  }
}

function onSessionUsePreferredAddress(address, port, family) {
  const session = this[owner_symbol];
  session[kUsePreferredAddress]({
    address,
    port,
    type: family === AF_INET6 ? 'udp6' : 'udp4'
  });
}

// Called by the C++ internals to emit a QLog record. This can
// be called before the QuicSession has been fully initialized,
// in which case we store a reference and defer emitting the
// qlog event until after we're initialized.
function onSessionQlog(handle) {
  const session = this[owner_symbol];
  const stream = new QLogStream(handle);
  if (session)
    session[kSetQLogStream](stream);
  else
    this.qlogStream = stream;
}

// Called by the C++ internals when a client QuicSession receives
// a version negotiation response from the server.
function onSessionVersionNegotiation(
  version,
  requestedVersions,
  supportedVersions) {
  if (this[owner_symbol]) {
    this[owner_symbol][kVersionNegotiation](
      version,
      requestedVersions,
      supportedVersions);
  }
}

// Called by the C++ internals to emit keylogging details for a
// QuicSession.
function onSessionKeylog(line) {
  if (this[owner_symbol]) {
    this[owner_symbol].emit('keylog', line);
  }
}

// Called by the C++ internals when a new QuicStream has been created.
function onStreamReady(streamHandle, id, push_id) {
  const session = this[owner_symbol];

  // onStreamReady should never be called if the stream is in a closing
  // state because new streams should not have been accepted at the C++
  // level.
  assert(!session.closing);
  const stream = new QuicStream({
    ...session[kStreamOptions],
    writable: !(id & 0b10),
  }, session, streamHandle, push_id);
  process.nextTick(() => {
    try {
      session.emit('stream', stream);
    } catch (error) {
      stream.destroy(error);
    }
  });
}

// Called by the C++ internals when a stream is closed and
// needs to be destroyed on the JavaScript side.
function onStreamClose(id, appErrorCode) {
  this[owner_symbol][kStreamClose](id, appErrorCode);
}

// Called by the C++ internals when a stream has been reset
function onStreamReset(id, appErrorCode) {
  this[owner_symbol][kStreamReset](id, appErrorCode);
}

// Called when an error occurs in a QuicStream
function onStreamError(streamHandle, error) {
  streamHandle[owner_symbol].destroy(error);
}

// Called when a block of headers has been fully
// received for the stream. Not all QuicStreams
// will support headers. The headers argument
// here is an Array of name-value pairs.
function onStreamHeaders(id, headers, kind, push_id) {
  this[owner_symbol][kHeaders](id, headers, kind, push_id);
}

// When a stream is flow control blocked, causes a blocked event
// to be emitted. This is a purely informational event.
function onStreamBlocked() {
  process.nextTick(emit.bind(this[owner_symbol], 'blocked'));
}

// Register the callbacks with the QUIC internal binding.
setCallbacks({
  onSocketClose,
  onSocketServerBusy,
  onSessionReady,
  onSessionCert,
  onSessionStatus,
  onSessionClientHello,
  onSessionClose,
  onSessionHandshake,
  onSessionKeylog,
  onSessionQlog,
  onSessionTicket,
  onSessionVersionNegotiation,
  onStreamReady,
  onStreamClose,
  onStreamError,
  onStreamReset,
  onSessionPathValidation,
  onSessionUsePreferredAddress,
  onStreamHeaders,
  onStreamBlocked,
});

// Creates the SecureContext used by QuicSocket instances that are listening
// for new connections.
function createSecureContext(options, init_cb) {
  const sc_options = validateCreateSecureContextOptions(options);
  const { groups, earlyData } = sc_options;
  const sc = _createSecureContext(sc_options);
  // TODO(@jasnell): Determine if it's really necessary to pass in groups here.
  init_cb(sc.context, groups, earlyData);
  return sc;
}

function onNewListener(event) {
  toggleListeners(this[kInternalState].state, event, true);
}

function onRemoveListener(event) {
  toggleListeners(this[kInternalState].state, event, false);
}

function getStats(obj, idx) {
  const stats = obj[kHandle]?.stats || obj[kInternalState].stats;
  // If stats is undefined at this point, it's just a bug
  assert(stats);
  return stats[idx];
}

function addressOrLocalhost(address, type) {
  return address || (type === AF_INET6 ? '::' : '0.0.0.0');
}

function deferredClosePromise(state) {
  return state.closePromise = new Promise((resolve, reject) => {
    state.closePromiseResolve = resolve;
    state.closePromiseReject = reject;
  }).finally(() => {
    state.closePromise = undefined;
    state.closePromiseResolve = undefined;
    state.closePromiseReject = undefined;
  });
}

async function resolvePreferredAddress(lookup, preferredAddress) {
  if (preferredAddress === undefined)
    return {};
  const {
    address,
    port,
    type = 'udp4'
  } = { ...preferredAddress };
  const [typeVal] = getSocketType(type);
  const {
    address: ip
  } = await lookup(address, typeVal === AF_INET6 ? 6 : 4);
  return { ip, port, type };
}

// QuicEndpoint wraps a UDP socket and is owned
// by a QuicSocket. It does not exist independently
// of the QuicSocket.
class QuicEndpoint {
  [kInternalState] = {
    state: kSocketUnbound,
    bindPromise: undefined,
    closePromise: undefined,
    closePromiseResolve: undefined,
    closePromiseReject: undefined,
    socket: undefined,
    udpSocket: undefined,
    address: undefined,
    ipv6Only: undefined,
    lookup: undefined,
    port: undefined,
    reuseAddr: undefined,
    type: undefined,
    fd: undefined
  };

  constructor(socket, options) {
    const {
      address,
      ipv6Only,
      lookup,
      port = 0,
      reuseAddr,
      type,
      preferred,
    } = validateQuicEndpointOptions(options);
    const state = this[kInternalState];
    state.socket = socket;
    state.address = addressOrLocalhost(address, type);
    state.lookup = lookup;
    state.ipv6Only = ipv6Only;
    state.port = port;
    state.reuseAddr = reuseAddr;
    state.type = type;
    state.udpSocket = dgram.createSocket(type === AF_INET6 ? 'udp6' : 'udp4');

    // kUDPHandleForTesting is only used in the Node.js test suite to
    // artificially test the endpoint. This code path should never be
    // used in user code.
    if (typeof options[kUDPHandleForTesting] === 'object') {
      state.udpSocket.bind(options[kUDPHandleForTesting]);
      state.state = kSocketBound;
      state.socket[kEndpointBound](this);
    }
    const udpHandle = state.udpSocket[internalDgram.kStateSymbol].handle;
    const handle = new QuicEndpointHandle(socket[kHandle], udpHandle);
    handle[owner_symbol] = this;
    this[kHandle] = handle;
    socket[kHandle].addEndpoint(handle, preferred);
  }

  [kInspect](depth, options) {
    return customInspect(this, {
      address: this.address,
      fd: this.fd,
      type: this[kInternalState].type === AF_INET6 ? 'udp6' : 'udp4',
      destroyed: this.destroyed,
      bound: this.bound,
      pending: this.pending,
    }, depth, options);
  }

  bind(options) {
    const state = this[kInternalState];
    if (state.bindPromise !== undefined)
      return state.bindPromise;

    return state.bindPromise = this[kBind]().finally(() => {
      state.bindPromise = undefined;
    });
  }

  // Binds the QuicEndpoint to the local port. Returns a Promise
  // that is resolved once the QuicEndpoint binds, or rejects if
  // binding was not successful. Calling bind() multiple times
  // before the Promise is resolved will return the same Promise.
  // Calling bind() after the endpoint is already bound will
  // immediately return a resolved promise. Calling bind() after
  // the endpoint has been destroyed will cause the Promise to
  // be rejected.
  async [kBind](options) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint is already destroyed');

    if (state.state !== kSocketUnbound)
      return this.address;

    const { signal } = { ...options };
    if (signal != null && !('aborted' in signal))
      throw new ERR_INVALID_ARG_TYPE('options.signal', 'AbortSignal', signal);

    // If an AbotSignal was passed in, check to make sure it is not already
    // aborted before we continue on to do any work.
    if (signal && signal.aborted)
      throw new lazyDOMException('AbortError');

    state.state = kSocketPending;

    const {
      address: ip
    } = await state.lookup(state.address, state.type === AF_INET6 ? 6 : 4);

    // It's possible for the QuicEndpoint to have been destroyed while
    // we were waiting for the DNS lookup to complete. If so, reject
    // the Promise.
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint was destroyed');

    // If an AbortSignal was passed in, check to see if it was triggered
    // while we were waiting.
    if (signal && signal.aborted) {
      state.state = kSocketUnbound;
      throw new lazyDOMException('AbortError');
    }

    // From here on, any errors are fatal for the QuicEndpoint. Keep in
    // mind that this means that the Bind Promise will be rejected *and*
    // the QuicEndpoint will be destroyed with an error.
    try {
      const udpHandle = state.udpSocket[internalDgram.kStateSymbol].handle;
      if (udpHandle == null) {
        // It's not clear what cases trigger this but it is possible.
        throw new ERR_OPERATION_FAILED('Acquiring UDP socket handle failed');
      }

      const flags =
        (state.reuseAddr ? UV_UDP_REUSEADDR : 0) |
        (state.ipv6Only ? UV_UDP_IPV6ONLY : 0);

      const ret = udpHandle.bind(ip, state.port, flags);
      if (ret)
        throw exceptionWithHostPort(ret, 'bind', ip, state.port);

      // On Windows, the fd will be meaningless, but we always record it.
      state.fd = udpHandle.fd;
      state.state = kSocketBound;

      return this.address;
    } catch (error) {
      this.destroy(error);
      throw error;
    }
  }

  destroy(error) {
    if (this.destroyed)
      return;

    const state = this[kInternalState];
    state.state = kSocketDestroyed;

    const handle = this[kHandle];
    if (handle === undefined)
      return;

    this[kHandle] = undefined;
    handle[owner_symbol] = undefined;
    // Calling waitForPendingCallbacks starts the process of
    // closing down the QuicEndpoint. Once all pending writes
    // to the underlying libuv udp handle have completed, the
    // ondone callback will be invoked, triggering the UDP
    // socket to be closed. Once it is closed, we notify
    // the QuicSocket that this QuicEndpoint has been closed,
    // allowing it to be freed.
    handle.ondone = () => {
      state.udpSocket.close((err) => {
        if (err) error = err;
        if (error && typeof state.closePromiseReject === 'function')
          state.closePromiseReject(error);
        else if (typeof state.closePromiseResolve === 'function')
          state.closePromiseResolve();
        state.socket[kEndpointClose](this, error);
      });
    };
    handle.waitForPendingCallbacks();
  }

  // Closes the QuicEndpoint. Returns a Promise that is resolved
  // once the QuicEndpoint closes, or rejects if it closes with
  // an error. Calling close() multiple times before the Promise
  // is resolved will return the same Promise. Calling close()
  // after will return a rejected Promise.
  close() {
    return this[kInternalState].closePromise || this[kClose]();
  }

  [kClose]() {
    if (this.destroyed) {
      return PromiseReject(
        new ERR_INVALID_STATE('QuicEndpoint is already destroyed'));
    }
    const promise = deferredClosePromise(this[kInternalState]);
    this.destroy();
    return promise;
  }

  // If the QuicEndpoint is bound, returns an object detailing
  // the local IP address, port, and address type to which it
  // is bound. Otherwise, returns an empty object.
  get address() {
    const state = this[kInternalState];
    if (state.state !== kSocketDestroyed) {
      try {
        return state.udpSocket.address();
      } catch (err) {
        if (err.code === 'EBADF') {
          // If there is an EBADF error, the socket is not bound.
          // Return empty object. Else, rethrow the error because
          // something else bad happened.
          return {};
        }
        throw err;
      }
    }
    return {};
  }

  // On Windows, this always returns undefined.
  get fd() {
    return this[kInternalState].fd >= 0 ?
      this[kInternalState].fd : undefined;
  }

  // True if the QuicEndpoint has been destroyed and is
  // no longer usable.
  get destroyed() {
    return this[kInternalState].state === kSocketDestroyed;
  }

  // True if binding has been initiated and is in progress.
  get pending() {
    return this[kInternalState].state === kSocketPending;
  }

  // True if the QuicEndpoint has been bound to the localUDP port.
  get bound() {
    return this[kInternalState].state === kSocketBound;
  }

  setTTL(ttl) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint is already destroyed');
    state.udpSocket.setTTL(ttl);
    return this;
  }

  setMulticastTTL(ttl) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint is already destroyed');
    state.udpSocket.setMulticastTTL(ttl);
    return this;
  }

  setBroadcast(on = true) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint is already destroyed');
    state.udpSocket.setBroadcast(on);
    return this;
  }

  setMulticastLoopback(on = true) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint is already destroyed');
    state.udpSocket.setMulticastLoopback(on);
    return this;
  }

  setMulticastInterface(iface) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint is already destroyed');
    state.udpSocket.setMulticastInterface(iface);
    return this;
  }

  addMembership(address, iface) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint is already destroyed');
    state.udpSocket.addMembership(address, iface);
    return this;
  }

  dropMembership(address, iface) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint is already destroyed');
    state.udpSocket.dropMembership(address, iface);
    return this;
  }

  ref() {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint is already destroyed');
    state.udpSocket.ref();
    return this;
  }

  unref() {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicEndpoint is already destroyed');
    state.udpSocket.unref();
    return this;
  }
}

// QuicSocket wraps a UDP socket plus the associated TLS context and QUIC
// Protocol state. There may be *multiple* QUIC connections (QuicSession)
// associated with a single QuicSocket.
class QuicSocket extends EventEmitter {
  [kInternalState] = {
    alpn: undefined,
    bindPromise: undefined,
    client: undefined,
    closePromise: undefined,
    closePromiseResolve: undefined,
    closePromiseReject: undefined,
    defaultEncoding: undefined,
    endpoints: new Set(),
    highWaterMark: undefined,
    listenPending: false,
    listenPromise: undefined,
    lookup: undefined,
    ocspHandler: undefined,
    clientHelloHandler: undefined,
    server: undefined,
    serverSecureContext: undefined,
    sessions: new Set(),
    state: kSocketUnbound,
    sharedState: undefined,
    stats: undefined,
  };

  constructor(options) {
    const {
      endpoint,

      // Default configuration for QuicClientSessions
      client,

      // The maximum number of connections
      maxConnections,

      // The maximum number of connections per host
      maxConnectionsPerHost,

      // The maximum number of stateless resets per host
      maxStatelessResetsPerHost,

      // The maximum number of seconds for retry token
      retryTokenTimeout,

      // The DNS lookup function
      lookup,

      // Default configuration for QuicServerSessions
      server,

      // True if address verification should be used.
      validateAddress,

      // Whether qlog should be enabled for sessions
      qlog,

      // Stateless reset token secret (16 byte buffer)
      statelessResetSecret,

      // When true, stateless resets will not be sent (default false)
      disableStatelessReset,
    } = validateQuicSocketOptions(options);
    super({ captureRejections: true });

    const state = this[kInternalState];

    state.client = client;
    state.server = server;
    state.lookup = lookup;

    let socketOptions = 0;
    if (validateAddress)
      socketOptions |= (1 << QUICSOCKET_OPTIONS_VALIDATE_ADDRESS);

    this[kSetHandle](
      new QuicSocketHandle(
        socketOptions,
        retryTokenTimeout,
        maxConnections,
        maxConnectionsPerHost,
        maxStatelessResetsPerHost,
        qlog,
        statelessResetSecret,
        disableStatelessReset));

    this.addEndpoint({ ...endpoint, preferred: true });
  }

  [kRejections](err, eventname, ...args) {
    switch (eventname) {
      case 'session':
        const session = args[0];
        session.destroy(err);
        process.nextTick(() => {
          this.emit('sessionError', err, session);
        });
        return;
      default:
        // Fall through
    }
    this.destroy(err);
  }

  get [kStreamOptions]() {
    const state = this[kInternalState];
    return {
      highWaterMark: state.highWaterMark,
      defaultEncoding: state.defaultEncoding,
      ocspHandler: state.ocspHandler,
      clientHelloHandler: state.clientHelloHandler,
      context: state.serverSecureContext,
    };
  }

  [kSetHandle](handle) {
    this[kHandle] = handle;
    if (handle !== undefined) {
      handle[owner_symbol] = this;
      this[async_id_symbol] = handle.getAsyncId();
      this[kInternalState].sharedState =
        new QuicSocketSharedState(handle.state);
    } else {
      this[kInternalState].sharedState = undefined;
    }
  }

  [kInspect](depth, options) {
    const state = this[kInternalState];
    return customInspect(this, {
      endpoints: this.endpoints,
      sessions: Array.from(state.sessions),
      bound: this.bound,
      pending: this.pending,
      closing: this.closing,
      destroyed: this.destroyed,
      listening: this.listening,
      serverBusy: this.serverBusy,
      statelessResetDisabled: this.statelessResetDisabled,
    }, depth, options);
  }

  [kAddSession](session) {
    this[kInternalState].sessions.add(session);
  }

  [kRemoveSession](session) {
    const state = this[kInternalState];
    state.sessions.delete(session);
    if (this.closing && state.sessions.size === 0)
      this.destroy();
  }

  [kMaybeBind](options) {
    const state = this[kInternalState];
    if (state.bindPromise !== undefined)
      return state.bindPromise;

    return state.bindPromise = this[kBind](options).finally(() => {
      state.bindPromise = undefined;
    });
  }

  async [kBind](options) {
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket is already destroyed');

    const state = this[kInternalState];
    if (state.state === kSocketBound)
      return;

    const { signal } = { ...options };
    if (signal != null && !('aborted' in signal))
      throw new ERR_INVALID_ARG_TYPE('options.signal', 'AbortSignal', signal);

    // If an AbotSignal was passed in, check to make sure it is not already
    // aborted before we continue on to do any work.
    if (signal && signal.aborted)
      throw new lazyDOMException('AbortError');

    state.state = kSocketPending;

    const binds = [];
    for (const endpoint of state.endpoints)
      binds.push(endpoint.bind({ signal }));

    await PromiseAll(binds);

    // It it's possible that the QuicSocket was destroyed while we were
    // waiting. Check to make sure.
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket was destroyed');

    // Any errors from this point on are fatal for the QuicSocket.
    try {
      // If an AbortSignal is used and it has been triggered, our
      // only recourse at this point is to destroy() the QuicSocket.
      // Some number of endpoints may have successfully bound, while
      // others have not
      if (signal && signal.aborted)
        throw lazyDOMException('AbortError');

      state.state = kSocketBound;

      process.nextTick(() => {
        // User code may have run before this so we need to check the
        // destroyed state. If it has been destroyed, do nothing.
        if (this.destroyed)
          return;
        try {
          this.emit('ready');
        } catch (error) {
          this.destroy(error);
        }
      });
    } catch (error) {
      this.destroy(error);
      throw error;
    }
  }

  // Currently only used for testing when the QuicEndpoint is bound immediately.
  [kEndpointBound](endpoint) {
    const state = this[kInternalState];
    if (state.state === kSocketBound)
      return;
    state.state = kSocketBound;

    // The ready event indicates that the QuicSocket is ready to be
    // used to either listen or connect. No QuicServerSession should
    // exist before this event, and all QuicClientSession will remain
    // in Initial states until ready is invoked.
    process.nextTick(() => {
      try {
        this.emit('ready');
      } catch (error) {
        this.destroy(error);
      }
    });
  }

  // Called by the C++ internals to notify when server busy status is toggled.
  [kServerBusy]() {
    const busy = this.serverBusy;
    process.nextTick(() => {
      try {
        this.emit('busy', busy);
      } catch (error) {
        this[kRejections](error, 'busy', busy);
      }
    });
  }

  // A QuicSocket will typically bind only to a single local port, but it is
  // possible to bind to multiple, even if those use different IP families
  // (e.g. IPv4 or IPv6). Calls to addEndpoint() must be made before the
  // QuicSocket is bound (e.g. before any calls to listen() or connect()).
  addEndpoint(options = {}) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket is already destroyed');
    if (state.state !== kSocketUnbound)
      throw new ERR_INVALID_STATE('QuicSocket is already being bound');

    options = {
      lookup: state.lookup,
      ...options
    };

    const endpoint = new QuicEndpoint(this, options);
    state.endpoints.add(endpoint);
    return endpoint;
  }

  listen(options) {
    const state = this[kInternalState];
    if (state.listenPromise !== undefined)
      return state.listenPromise;

    return state.listenPromise = this[kListen](options).finally(() => {
      state.listenPromise = undefined;
    });
  }

  async [kListen](options) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket is already destroyed');
    if (this.closing)
      throw new ERR_INVALID_STATE('QuicSocket is closing');
    if (this.listening)
      throw new ERR_INVALID_STATE('QuicSocket is already listening');

    options = {
      ...state.server,
      ...options,
    };

    // The ALPN protocol identifier is strictly required.
    const {
      alpn,
      lookup = state.lookup,
      defaultEncoding,
      highWaterMark,
      ocspHandler,
      clientHelloHandler,
      transportParams,
    } = validateQuicSocketListenOptions(options);

    state.serverSecureContext =
      createSecureContext({
        ...options,
        minVersion: 'TLSv1.3',
        maxVersion: 'TLSv1.3',
      }, initSecureContext);
    state.highWaterMark = highWaterMark;
    state.defaultEncoding = defaultEncoding;
    state.alpn = alpn;
    state.listenPending = true;
    state.ocspHandler = ocspHandler;
    state.clientHelloHandler = clientHelloHandler;

    await this[kMaybeBind]();

    // It's possible that the QuicSocket was destroyed or closed while
    // the bind was pending. Check for that and handle accordingly.
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket was destroyed');
    if (this.closing)
      throw new ERR_INVALID_STATE('QuicSocket is closing');

    const {
      ip,
      port,
      type
    } = await resolvePreferredAddress(lookup, transportParams.preferredAddress);

    // It's possible that the QuicSocket was destroyed or closed while
    // the preferred address resolution was pending. Check for that and handle
    // accordingly.
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket was destroyed');
    if (this.closing)
      throw new ERR_INVALID_STATE('QuicSocket is closing');

    const {
      rejectUnauthorized = !getAllowUnauthorized(),
      requestCert = false,
    } = transportParams;

    // Transport Parameters are passed to the C++ side using a shared array.
    // These are the transport parameters that will be used when a new
    // server QuicSession is established. They are transmitted to the client
    // as part of the server's initial TLS handshake. Once they are set, they
    // cannot be modified.
    setTransportParams(transportParams);

    // When the handle is told to listen, it will begin acting as a QUIC
    // server and will emit session events whenever a new QuicServerSession
    // is created.
    state.listenPending = false;
    this[kHandle].listen(
      state.serverSecureContext.context,
      ip,         // Preferred address ip,
      type,       // Preferred address type,
      port,       // Preferred address port,
      state.alpn,
      (rejectUnauthorized ? QUICSERVERSESSION_OPTION_REJECT_UNAUTHORIZED : 0) |
      (requestCert ? QUICSERVERSESSION_OPTION_REQUEST_CERT : 0));

    process.nextTick(() => {
      // It's remotely possible the QuicSocket is be destroyed or closed
      // while the nextTick is pending. If that happens, do nothing.
      if (this.destroyed || this.closing)
        return;
      try {
        this.emit('listening');
      } catch (error) {
        this.destroy(error);
      }
    });
  }

  async connect(options) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket is already destroyed');
    if (this.closing)
      throw new ERR_INVALID_STATE('QuicSocket is closing');

    options = {
      ...state.client,
      ...options
    };

    const {
      type,
      address,
      lookup = state.lookup
    } = validateQuicSocketConnectOptions(options);

    await this[kMaybeBind]();

    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket was destroyed');
    if (this.closing)
      throw new ERR_INVALID_STATE('QuicSocket is closing');

    const {
      address: ip
    } = await lookup(addressOrLocalhost(address, type),
                     type === AF_INET6 ? 6 : 4);

    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket was destroyed');
    if (this.closing)
      throw new ERR_INVALID_STATE('QuicSocket is closing');

    return new QuicClientSession(this, options, type, ip);
  }

  [kEndpointClose](endpoint, error) {
    const state = this[kInternalState];
    state.endpoints.delete(endpoint);
    process.nextTick(() => {
      try {
        this.emit('endpointClose', endpoint, error);
      } catch (error) {
        this.destroy(error);
      }
    });

    // If there aren't any more endpoints, the QuicSession
    // is no longer usable and needs to be destroyed.
    if (state.endpoints.size === 0) {
      if (!this.destroyed)
        return this.destroy(error);
      this[kDestroy](error);
    }
  }

  // Initiate a Graceful Close of the QuicSocket.
  // Existing QuicClientSession and QuicServerSession instances will be
  // permitted to close naturally and gracefully on their own.
  // The QuicSocket will be immediately closed and freed as soon as there
  // are no additional session instances remaining. If there are no
  // QuicClientSession or QuicServerSession instances, the QuicSocket
  // will be immediately closed.
  //
  // Returns a Promise that will be resolved once the QuicSocket is
  // destroyed.
  //
  // No additional QuicServerSession instances will be accepted from
  // remote peers, and calls to connect() to create QuicClientSession
  // instances will fail. The QuicSocket will be otherwise usable in
  // every other way.
  //
  // Once initiated, a graceful close cannot be canceled. The graceful
  // close can be interupted, however, by abruptly destroying the
  // QuicSocket using the destroy() method.
  //
  // If close() is called before the QuicSocket has been bound (before
  // either connect() or listen() have been called, or the QuicSocket
  // is still in the pending state, the QuicSocket is destroyed
  // immediately.
  close() {
    return this[kInternalState].closePromise || this[kClose]();
  }

  [kClose]() {
    if (this.destroyed) {
      return PromiseReject(
        new ERR_INVALID_STATE('QuicSocket is already destroyed'));
    }
    const state = this[kInternalState];
    const promise = deferredClosePromise(state);

    // Tell the underlying QuicSocket C++ object to stop
    // listening for new QuicServerSession connections.
    // New initial connection packets for currently unknown
    // DCID's will be ignored.
    if (this[kHandle])
      state.sharedState.serverListening = false;

    // If the QuicSocket is otherwise not bound to the local
    // port, or there are not active sessions, destroy the
    // QuicSocket immediately and we're done.
    if (state.state !== kSocketBound || state.sessions.size === 0) {
      this.destroy();
      return promise;
    }

    // Otherwise, loop through each of the known sessions and close them.
    const reqs = [promise];
    for (const session of state.sessions) {
      reqs.push(session.close()
                       .catch((error) => this.destroy(error)));
    }
    return PromiseAll(reqs);
  }

  // Initiate an abrupt close and destruction of the QuicSocket.
  // Existing QuicClientSession and QuicServerSession instances will be
  // immediately closed. If error is specified, it will be forwarded
  // to each of the session instances.
  //
  // When the session instances are closed, an attempt to send a final
  // CONNECTION_CLOSE will be made.
  //
  // The JavaScript QuicSocket object will be marked destroyed and will
  // become unusable. As soon as all pending outbound UDP packets are
  // flushed from the QuicSocket's queue, the QuicSocket C++ instance
  // will be destroyed and freed from memory.
  destroy(error) {
    const state = this[kInternalState];
    // If the QuicSocket is already destroyed, do nothing
    if (state.state === kSocketDestroyed)
      return;

    // Mark the QuicSocket as being destroyed.
    state.state = kSocketDestroyed;
    this[kHandle].stats[IDX_QUIC_SOCKET_STATS_DESTROYED_AT] =
      process.hrtime.bigint();
    state.stats = new BigInt64Array(this[kHandle].stats);

    // Immediately close any sessions that may be remaining.
    // If the udp socket is in a state where it is able to do so,
    // a final attempt to send CONNECTION_CLOSE frames for each
    // closed session will be made.
    for (const session of state.sessions)
      session.destroy(error);

    // If there aren't any QuicEndpoints to clean up, skip
    // directly to the end to emit the error and close events.
    if (state.endpoints.size === 0)
      return this[kDestroy](error);

    // Otherwise, the QuicSocket will be destroyed once all
    // QuicEndpoints are destroyed. See [kEndpointClose].
    for (const endpoint of state.endpoints)
      endpoint.destroy(error);
  }

  [kDestroy](error) {
    const state = this[kInternalState];
    if (error) {
      if (typeof state.closePromiseReject === 'function')
        state.closePromiseReject(error);
      process.nextTick(emit.bind(this, 'error', error));
    } else if (typeof state.closePromiseResolve === 'function') {
      state.closePromiseResolve();
    }
    process.nextTick(emit.bind(this, 'close'));
  }

  ref() {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket is already destroyed');
    for (const endpoint of state.endpoints)
      endpoint.ref();
    return this;
  }

  unref() {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket is already destroyed');
    for (const endpoint of state.endpoints)
      endpoint.unref();
    return this;
  }

  get endpoints() {
    return Array.from(this[kInternalState].endpoints);
  }

  get serverSecureContext() {
    return this[kInternalState].serverSecureContext;
  }

  // True if at least one associated QuicEndpoint has been successfully
  // bound to a local UDP port.
  get bound() {
    return this[kInternalState].state === kSocketBound;
  }

  // True if graceful close has been initiated by calling close()
  get closing() {
    return this[kInternalState].closePromise !== undefined;
  }

  // True if the QuicSocket has been destroyed and is no longer usable
  get destroyed() {
    return this[kInternalState].state === kSocketDestroyed;
  }

  // True if listen() has been called successfully
  get listening() {
    return Boolean(this[kInternalState].sharedState?.serverListening);
  }

  // True if the QuicSocket is currently waiting on at least one
  // QuicEndpoint to succesfully bind.g
  get pending() {
    return this[kInternalState].state === kSocketPending;
  }

  // Marking a server as busy will cause all new
  // connection attempts to fail with a SERVER_BUSY CONNECTION_CLOSE.
  set serverBusy(on) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket is already destroyed');
    validateBoolean(on, 'on');
    if (state.sharedState.serverBusy !== on) {
      state.sharedState.serverBusy = on;
      this[kServerBusy]();
    }
  }

  get serverBusy() {
    return Boolean(this[kInternalState].sharedState?.serverBusy);
  }

  set statelessResetDisabled(on) {
    const state = this[kInternalState];
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket is already destroyed');
    validateBoolean(on, 'on');
    if (state.sharedState.statelessResetDisabled !== on)
      state.sharedState.statelessResetDisabled = on;
  }

  get statelessResetDisabled() {
    return Boolean(this[kInternalState].sharedState?.statelessResetDisabled);
  }

  get duration() {
    const end = getStats(this, IDX_QUIC_SOCKET_STATS_DESTROYED_AT) ||
                process.hrtime.bigint();
    return Number(end - getStats(this, IDX_QUIC_SOCKET_STATS_CREATED_AT));
  }

  get boundDuration() {
    const end = getStats(this, IDX_QUIC_SOCKET_STATS_DESTROYED_AT) ||
                process.hrtime.bigint();
    return Number(end - getStats(this, IDX_QUIC_SOCKET_STATS_BOUND_AT));
  }

  get listenDuration() {
    const end = getStats(this, IDX_QUIC_SOCKET_STATS_DESTROYED_AT) ||
                process.hrtime.bigint();
    return Number(end - getStats(this, IDX_QUIC_SOCKET_STATS_LISTEN_AT));
  }

  get bytesReceived() {
    return Number(getStats(this, IDX_QUIC_SOCKET_STATS_BYTES_RECEIVED));
  }

  get bytesSent() {
    return Number(getStats(this, IDX_QUIC_SOCKET_STATS_BYTES_SENT));
  }

  get packetsReceived() {
    return Number(getStats(this, IDX_QUIC_SOCKET_STATS_PACKETS_RECEIVED));
  }

  get packetsSent() {
    return Number(getStats(this, IDX_QUIC_SOCKET_STATS_PACKETS_SENT));
  }

  get packetsIgnored() {
    return Number(getStats(this, IDX_QUIC_SOCKET_STATS_PACKETS_IGNORED));
  }

  get serverSessions() {
    return Number(getStats(this, IDX_QUIC_SOCKET_STATS_SERVER_SESSIONS));
  }

  get clientSessions() {
    return Number(getStats(this, IDX_QUIC_SOCKET_STATS_CLIENT_SESSIONS));
  }

  get statelessResetCount() {
    return Number(getStats(this, IDX_QUIC_SOCKET_STATS_STATELESS_RESET_COUNT));
  }

  get serverBusyCount() {
    return Number(getStats(this, IDX_QUIC_SOCKET_STATS_SERVER_BUSY_COUNT));
  }

  // Diagnostic packet loss is a testing mechanism that allows simulating
  // pseudo-random packet loss for rx or tx. The value specified for each
  // option is a number between 0 and 1 that identifies the possibility of
  // packet loss in the given direction.
  setDiagnosticPacketLoss(options) {
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket is already destroyed');
    const {
      rx = 0.0,
      tx = 0.0
    } = { ...options };
    validateNumber(
      rx,
      'options.rx',
      /* min */ 0.0,
      /* max */ 1.0);
    validateNumber(
      tx,
      'options.tx',
      /* min */ 0.0,
      /* max */ 1.0);
    if ((rx > 0.0 || tx > 0.0) && !diagnosticPacketLossWarned) {
      diagnosticPacketLossWarned = true;
      process.emitWarning(
        'QuicSocket diagnostic packet loss is enabled. Received or ' +
        'transmitted packets will be randomly ignored to simulate ' +
        'network packet loss.');
    }
    this[kHandle].setDiagnosticPacketLoss(rx, tx);
  }
}

class QuicSession extends EventEmitter {
  [kInternalState] = {
    alpn: undefined,
    cipher: undefined,
    cipherVersion: undefined,
    clientHelloHandler: undefined,
    closeCode: NGTCP2_NO_ERROR,
    closeFamily: QUIC_ERROR_APPLICATION,
    closePromise: undefined,
    closePromiseResolve: undefined,
    closePromiseReject: undefined,
    destroyed: false,
    earlyData: false,
    handshakeComplete: false,
    handshakeCompletePromise: undefined,
    handshakeCompletePromiseResolve: undefined,
    handshakeCompletePromiseReject: undefined,
    idleTimeout: false,
    maxPacketLength: NGTCP2_DEFAULT_MAX_PKTLEN,
    ocspHandler: undefined,
    servername: undefined,
    socket: undefined,
    silentClose: false,
    statelessReset: false,
    stats: undefined,
    streams: new Map(),
    verifyErrorReason: undefined,
    verifyErrorCode: undefined,
    handshakeAckHistogram: undefined,
    handshakeContinuationHistogram: undefined,
    highWaterMark: undefined,
    defaultEncoding: undefined,
    state: undefined,
    qlogStream: undefined,
  };

  constructor(socket, options) {
    const {
      alpn,
      servername,
      highWaterMark,
      defaultEncoding,
      ocspHandler,
      clientHelloHandler,
    } = options;
    super({ captureRejections: true });
    this.on('newListener', onNewListener);
    this.on('removeListener', onRemoveListener);
    const state = this[kInternalState];
    state.socket = socket;
    state.servername = servername;
    state.alpn = alpn;
    state.highWaterMark = highWaterMark;
    state.defaultEncoding = defaultEncoding;
    state.ocspHandler = ocspHandler;
    state.clientHelloHandler = clientHelloHandler;
    socket[kAddSession](this);
  }

  [kRejections](err, eventname, ...args) {
    this.destroy(err);
  }

  // Used to get the configured options for peer initiated QuicStream
  // instances.
  get [kStreamOptions]() {
    const state = this[kInternalState];
    return {
      highWaterMark: state.highWaterMark,
      defaultEncoding: state.defaultEncoding,
    };
  }

  [kSetQLogStream](stream) {
    const state = this[kInternalState];
    state.qlogStream = stream;
    process.nextTick(() => {
      try {
        this.emit('qlog', state.qlogStream);
      } catch (error) {
        this.destroy(error);
      }
    });
  }

  [kHandshakeComplete]() {
    const state = this[kInternalState];
    if (state.handshakeComplete)
      return PromiseResolve();

    if (state.handshakeCompletePromise !== undefined)
      return state.handshakeCompletePromise;

    state.handshakeCompletePromise = new Promise((resolve, reject) => {
      state.handshakeCompletePromiseResolve = resolve;
      state.handshakeCompletePromiseReject = reject;
    }).finally(() => {
      state.handshakeCompletePromise = undefined;
      state.handshakeCompletePromiseReject = undefined;
      state.handshakeCompletePromiseResolve = undefined;
    });

    return state.handshakeCompletePromise;
  }

  // Sets the internal handle for the QuicSession instance. For
  // server QuicSessions, this is called immediately as the
  // handle is created before the QuicServerSession JS object.
  // For client QuicSession instances, the connect() method
  // must first perform DNS resolution on the provided address
  // before the underlying QuicSession handle can be created.
  [kSetHandle](handle) {
    const state = this[kInternalState];
    this[kHandle] = handle;
    if (handle !== undefined) {
      handle[owner_symbol] = this;
      state.state = new QuicSessionSharedState(handle.state);
      state.handshakeAckHistogram = new Histogram(handle.ack);
      state.handshakeContinuationHistogram = new Histogram(handle.rate);
      state.state.ocspEnabled = state.ocspHandler !== undefined;
      state.state.clientHelloEnabled = state.clientHelloHandler !== undefined;
      if (handle.qlogStream !== undefined) {
        this[kSetQLogStream](handle.qlogStream);
        handle.qlogStream = undefined;
      }
    } else {
      if (state.handshakeAckHistogram)
        state.handshakeAckHistogram[kDestroyHistogram]();
      if (state.handshakeContinuationHistogram)
        state.handshakeContinuationHistogram[kDestroyHistogram]();
    }
  }

  // Called when a client QuicSession instance receives a version
  // negotiation packet from the server peer. The client QuicSession
  // is destroyed immediately. This is not called at all for server
  // QuicSessions.
  [kVersionNegotiation](version, requestedVersions, supportedVersions) {
    const err =
      new ERR_QUIC_VERSION_NEGOTIATION(
        version,
        requestedVersions,
        supportedVersions);
    err.detail = {
      version,
      requestedVersions,
      supportedVersions,
    };
    this.destroy(err);
  }

  // Closes the specified stream with the given code. The
  // QuicStream object will be destroyed.
  [kStreamClose](id, code) {
    const stream = this[kInternalState].streams.get(id);
    if (stream === undefined)
      return;
    stream[kDestroy](code);
  }

  [kStreamReset](id, code) {
    const stream = this[kInternalState].streams.get(id);
    if (stream === undefined)
      return;

    stream[kStreamReset](code);
  }

  // Delivers a block of headers to the appropriate QuicStream
  // instance. This will only be called if the ALPN selected
  // is known to support headers.
  [kHeaders](id, headers, kind, push_id) {
    const stream = this[kInternalState].streams.get(id);
    if (stream === undefined)
      return;

    stream[kHeaders](headers, kind, push_id);
  }

  [kInspect](depth, options) {
    const state = this[kInternalState];
    return customInspect(this, {
      alpn: state.alpn,
      cipher: this.cipher,
      closing: this.closing,
      closeCode: this.closeCode,
      destroyed: this.destroyed,
      earlyData: state.earlyData,
      maxStreams: this.maxStreams,
      servername: this.servername,
      streams: state.streams.size,
    }, depth, options);
  }

  [kSetSocket](socket) {
    this[kInternalState].socket = socket;
  }

  // Called at the completion of the TLS handshake for the local peer
  [kHandshake](
    servername,
    alpn,
    cipher,
    cipherVersion,
    maxPacketLength,
    verifyErrorReason,
    verifyErrorCode,
    earlyData) {
    const state = this[kInternalState];
    state.handshakeComplete = true;
    state.servername = servername;
    state.alpn = alpn;
    state.cipher = cipher;
    state.cipherVersion = cipherVersion;
    state.maxPacketLength = maxPacketLength;
    state.verifyErrorReason = verifyErrorReason;
    state.verifyErrorCode = verifyErrorCode;
    state.earlyData = earlyData;

    if (!this[kHandshakePost]()) {
      if (typeof state.handshakeCompletePromiseReject === 'function') {
        state.handshakeCompletePromiseReject(
          new ERR_OPERATION_FAILED('Handshake failed'));
      }
      return;
    }

    if (typeof state.handshakeCompletePromiseResolve === 'function')
      state.handshakeCompletePromiseResolve();

    process.nextTick(() => {
      try {
        this.emit('secure', servername, alpn, this.cipher);
      } catch (error) {
        this.destroy(error);
      }
    });
  }

  // Non-op for the default case. QuicClientSession
  // overrides this with some client-side specific
  // checks
  [kHandshakePost]() {
    return true;
  }

  [kRemoveStream](stream) {
    this[kInternalState].streams.delete(stream.id);
    this[kMaybeDestroy]();
  }

  [kAddStream](id, stream) {
    this[kInternalState].streams.set(id, stream);
  }

  // Called when a client QuicSession has opted to use the
  // server provided preferred address. This is a purely
  // informationational notification. It is not called on
  // server QuicSession instances.
  [kUsePreferredAddress](address) {
    process.nextTick(() => {
      try {
        this.emit('usePreferredAddress', address);
      } catch (error) {
        this.destroy(error);
      }
    });
  }

  close() {
    return this[kInternalState].closePromise || this[kClose]();
  }

  [kClose]() {
    if (this.destroyed) {
      return PromiseReject(
        new ERR_INVALID_STATE('QuicSession is already destroyed'));
    }
    const promise = deferredClosePromise(this[kInternalState]);
    if (!this[kMaybeDestroy]()) {
      this[kHandle].gracefulClose();
    }
    return promise;
  }

  get closing() {
    return this[kInternalState].closePromise !== undefined;
  }

  // The QuicSession will be destroyed if close() has been
  // called and there are no remaining streams
  [kMaybeDestroy]() {
    const state = this[kInternalState];
    if (this.closing && state.streams.size === 0) {
      this.destroy();
      return true;
    }
    return false;
  }

  // Causes the QuicSession to be immediately destroyed, but with
  // additional metadata set.
  [kDestroy](code, family, silent, statelessReset) {
    const state = this[kInternalState];
    state.closeCode = code;
    state.closeFamily = family;
    state.silentClose = silent;
    state.statelessReset = statelessReset;
    this.destroy();
  }

  // Destroying synchronously shuts down and frees the
  // QuicSession immediately, even if there are still open
  // streams.
  //
  // Unless we're in the middle of a silent close, a
  // CONNECTION_CLOSE packet will be sent to the connected
  // peer and the session will be immediately destroyed.
  //
  // If destroy is called with an error argument, the
  // 'error' event is emitted on next tick.
  //
  // Once destroyed, and after the 'error' event (if any),
  // the 'close' event is emitted on next tick.
  destroy(error) {
    const state = this[kInternalState];
    // Destroy can only be called once. Multiple calls will be ignored
    if (state.destroyed)
      return;
    state.destroyed = true;

    state.idleTimeout = Boolean(this[kInternalState].state?.idleTimeout);

    // Destroy any remaining streams immediately.
    for (const stream of state.streams.values())
      stream.destroy(error);

    this.removeListener('newListener', onNewListener);
    this.removeListener('removeListener', onRemoveListener);

    const handle = this[kHandle];
    this[kHandle] = undefined;
    if (handle !== undefined) {
      // Copy the stats for use after destruction
      handle.stats[IDX_QUIC_SESSION_STATS_DESTROYED_AT] =
        process.hrtime.bigint();
      state.stats = new BigInt64Array(handle.stats);

      // Destroy the underlying QuicSession handle
      handle.destroy(state.closeCode, state.closeFamily);
    }

    // Remove the QuicSession JavaScript object from the
    // associated QuicSocket.
    state.socket[kRemoveSession](this);
    state.socket = undefined;

    // If we are destroying with an error, schedule the
    // error to be emitted on process.nextTick.
    if (error) {
      if (typeof state.closePromiseReject === 'function')
        state.closePromiseReject(error);
      process.nextTick(emit.bind(this, 'error', error));
    } else if (typeof state.closePromiseResolve === 'function')
      state.closePromiseResolve();

    if (typeof state.handshakeCompletePromiseReject === 'function') {
      state.handshakeCompletePromiseReject(
        new ERR_OPERATION_FAILED('Handshake failed'));
    }

    process.nextTick(emit.bind(this, 'close'));
  }

  // For server QuicSession instances, true if earlyData is
  // enabled. For client QuicSessions, true only if session
  // resumption is used and early data was accepted during
  // the TLS handshake. The value is set only after the
  // TLS handshake is completed (immeditely before the
  // secure event is emitted)
  get usingEarlyData() {
    return this[kInternalState].earlyData;
  }

  get maxStreams() {
    let bidi = 0;
    let uni = 0;
    if (this[kHandle]) {
      bidi = this[kInternalState].state.maxStreamsBidi;
      uni = this[kInternalState].state.maxStreamsUni;
    }
    return { bidi, uni };
  }

  get qlog() {
    return this[kInternalState].qlogStream;
  }

  get address() {
    return this[kInternalState].socket?.address || {};
  }

  get maxDataLeft() {
    return Number(this[kHandle] ? this[kInternalState].state.maxDataLeft : 0);
  }

  get bytesInFlight() {
    return Number(this[kHandle] ? this[kInternalState].state.bytesInFlight : 0);
  }

  get blockCount() {
    return Number(
      this[kHandle]?.stats[IDX_QUIC_SESSION_STATS_BLOCK_COUNT] || 0);
  }

  get authenticated() {
    // Specifically check for null. Undefined means the check has not
    // been performed yet, another other value other than null means
    // there was an error
    return this[kInternalState].verifyErrorReason == null;
  }

  get authenticationError() {
    if (this.authenticated)
      return undefined;
    const state = this[kInternalState];
    // eslint-disable-next-line no-restricted-syntax
    const err = new Error(state.verifyErrorReason);
    const code = 'ERR_QUIC_VERIFY_' + state.verifyErrorCode;
    err.name = `Error [${code}]`;
    err.code = code;
    return err;
  }

  get remoteAddress() {
    const out = {};
    if (this[kHandle])
      this[kHandle].getRemoteAddress(out);
    return out;
  }

  get handshakeComplete() {
    return this[kInternalState].handshakeComplete;
  }

  get handshakeConfirmed() {
    return this[kHandle] ?
      this[kInternalState].state.handshakeConfirmed :
      false;
  }

  get idleTimeout() {
    return this[kInternalState].idleTimeout;
  }

  get alpnProtocol() {
    return this[kInternalState].alpn;
  }

  get cipher() {
    if (!this.handshakeComplete)
      return {};
    const state = this[kInternalState];
    return {
      name: state.cipher,
      version: state.cipherVersion,
    };
  }

  getCertificate() {
    return this[kHandle] ?
      translatePeerCertificate(this[kHandle].getCertificate() || {}) : {};
  }

  getPeerCertificate(detailed = false) {
    return this[kHandle] ?
      translatePeerCertificate(
        this[kHandle].getPeerCertificate(detailed) || {}) : {};
  }

  ping() {
    if (this.destroyed) {
      throw new ERR_INVALID_STATE(
        `${this.constructor.name} is already destroyed`);
    }
    this[kHandle].ping();
  }

  get servername() {
    return this[kInternalState].servername;
  }

  get destroyed() {
    return this[kInternalState].destroyed;
  }

  get closeCode() {
    const state = this[kInternalState];
    return {
      code: state.closeCode,
      family: state.closeFamily,
      silent: state.silentClose,
    };
  }

  get socket() {
    return this[kInternalState].socket;
  }

  get statelessReset() {
    return this[kInternalState].statelessReset;
  }

  async openStream(options) {
    if (this.destroyed) {
      throw new ERR_INVALID_STATE(
        `${this.constructor.name} is already destroyed`);
    }
    if (this.closing) {
      throw new ERR_INVALID_STATE(
        `${this.constructor.name} is closing`);
    }

    const {
      halfOpen,  // Unidirectional or Bidirectional
      highWaterMark,
      defaultEncoding,
    } = validateQuicStreamOptions(options);

    const stream = new QuicStream({
      highWaterMark,
      defaultEncoding,
      readable: !halfOpen
    }, this);

    await once(stream, kReady);

    return stream;
  }

  get duration() {
    const end = getStats(this, IDX_QUIC_SESSION_STATS_DESTROYED_AT) ||
                process.hrtime.bigint();
    return Number(end - getStats(this, IDX_QUIC_SESSION_STATS_CREATED_AT));
  }

  get handshakeDuration() {
    const end =
      this.handshakeComplete ?
        getStats(this, IDX_QUIC_SESSION_STATS_HANDSHAKE_COMPLETED_AT) :
        process.hrtime.bigint();
    return Number(
      end - getStats(this, IDX_QUIC_SESSION_STATS_HANDSHAKE_START_AT));
  }

  get bytesReceived() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_BYTES_RECEIVED));
  }

  get bytesSent() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_BYTES_SENT));
  }

  get bidiStreamCount() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_BIDI_STREAM_COUNT));
  }

  get uniStreamCount() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_UNI_STREAM_COUNT));
  }

  get maxInFlightBytes() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_MAX_BYTES_IN_FLIGHT));
  }

  get lossRetransmitCount() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_LOSS_RETRANSMIT_COUNT));
  }

  get ackDelayRetransmitCount() {
    return Number(
      getStats(this, IDX_QUIC_SESSION_STATS_ACK_DELAY_RETRANSMIT_COUNT));
  }

  get peerInitiatedStreamCount() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_STREAMS_IN_COUNT));
  }

  get selfInitiatedStreamCount() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_STREAMS_OUT_COUNT));
  }

  get keyUpdateCount() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_KEYUPDATE_COUNT));
  }

  get minRTT() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_MIN_RTT));
  }

  get latestRTT() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_LATEST_RTT));
  }

  get smoothedRTT() {
    return Number(getStats(this, IDX_QUIC_SESSION_STATS_SMOOTHED_RTT));
  }

  updateKey() {
    const state = this[kInternalState];
    // Initiates a key update for the connection.
    if (this.destroyed) {
      throw new ERR_INVALID_STATE(
        `${this.constructor.name} is already destroyed`);
    }
    if (this.closing) {
      throw new ERR_INVALID_STATE(
        `${this.constructor.name} is closing`);
    }
    if (!state.handshakeConfirmed)
      throw new ERR_INVALID_STATE('Handshake is not yet confirmed');
    return this[kHandle].updateKey();
  }

  get handshakeAckHistogram() {
    return this[kInternalState].handshakeAckHistogram;
  }

  get handshakeContinuationHistogram() {
    return this[kInternalState].handshakeContinuationHistogram;
  }

  [kRemoveFromSocket]() {
    return this[kHandle].removeFromSocket();
  }
}
class QuicServerSession extends QuicSession {
  [kInternalServerState] = {
    context: undefined
  };

  constructor(socket, handle, options) {
    const {
      highWaterMark,
      defaultEncoding,
      ocspHandler,
      clientHelloHandler,
      context,
    } = options;
    super(socket, {
      highWaterMark,
      defaultEncoding,
      ocspHandler,
      clientHelloHandler
    });
    this[kInternalServerState].context = context;
    this[kSetHandle](handle);
  }

  // Called only when a clientHello event handler is registered.
  // Allows user code an opportunity to interject into the start
  // of the TLS handshake.
  async [kClientHello](alpn, servername, ciphers) {
    const internalState = this[kInternalState];
    return internalState.clientHelloHandler?.(alpn, servername, ciphers);
  }

  async [kHandleOcsp](servername) {
    const internalState = this[kInternalState];
    const { context } = this[kInternalServerState];
    if (!internalState.ocspHandler || !context) return undefined;
    return internalState.ocspHandler('request', {
      servername,
      certificate: context.context.getCertificate(),
      issuer: context.context.getIssuer()
    });
  }

  get allowEarlyData() { return false; }
}

class QuicClientSession extends QuicSession {
  [kInternalClientState] = {
    allowEarlyData: false,
    handshakeStarted: false,
    minDHSize: undefined,
    secureContext: undefined,
  };

  constructor(socket, options, type, ip) {
    const sc_options = {
      ...options,
      minVersion: 'TLSv1.3',
      maxVersion: 'TLSv1.3',
    };
    const {
      autoStart,
      alpn,
      dcid,
      minDHSize,
      ocspHandler,
      port,
      preferredAddressPolicy,
      remoteTransportParams,
      servername,
      sessionTicket,
      verifyHostnameIdentity,
      qlog,
      highWaterMark,
      defaultEncoding,
    } = validateQuicClientSessionOptions(options);

    if (!verifyHostnameIdentity && !warnedVerifyHostnameIdentity) {
      warnedVerifyHostnameIdentity = true;
      process.emitWarning(
        'QUIC hostname identity verification is disabled. This violates QUIC ' +
        'specification requirements and reduces security. Hostname identity ' +
        'verification should only be disabled for debugging purposes.'
      );
    }

    super(socket, {
      servername,
      alpn,
      highWaterMark,
      defaultEncoding,
      ocspHandler
    });
    const state = this[kInternalClientState];
    state.handshakeStarted = autoStart;
    state.minDHSize = minDHSize;

    state.secureContext =
      createSecureContext(
        sc_options,
        initSecureContextClient);

    const transportParams = validateTransportParams(options);

    state.allowEarlyData =
      remoteTransportParams !== undefined &&
      sessionTicket !== undefined;

    setTransportParams(transportParams);

    const handle =
      _createClientSession(
        this.socket[kHandle],
        type,
        ip,
        port,
        state.secureContext.context,
        this.servername || ip,
        remoteTransportParams,
        sessionTicket,
        dcid,
        preferredAddressPolicy,
        this.alpnProtocol,
        (verifyHostnameIdentity ?
          QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY : 0) |
        (ocspHandler !== undefined ?
          QUICCLIENTSESSION_OPTION_REQUEST_OCSP : 0),
        qlog,
        autoStart);

    // If handle is a number, creating the session failed.
    if (typeof handle === 'number') {
      let reason;
      switch (handle) {
        case ERR_FAILED_TO_CREATE_SESSION:
          throw new ERR_QUIC_FAILED_TO_CREATE_SESSION();
        case ERR_INVALID_REMOTE_TRANSPORT_PARAMS:
          throw new ERR_QUIC_INVALID_REMOTE_TRANSPORT_PARAMS();
        case ERR_INVALID_TLS_SESSION_TICKET:
          throw new ERR_QUIC_INVALID_TLS_SESSION_TICKET();
        default:
          throw new ERR_OPERATION_FAILED(`Unspecified reason [${reason}]`);
      }
    }

    this[kSetHandle](handle);
  }

  [kHandshakePost]() {
    const { type, size } = this.ephemeralKeyInfo;
    if (type === 'DH' && size < this[kInternalClientState].minDHSize) {
      this.destroy(new ERR_TLS_DH_PARAM_SIZE(size));
      return false;
    }
    return true;
  }

  async [kHandleOcsp](data) {
    const internalState = this[kInternalState];
    return internalState.ocspHandler?.('response', { data });
  }

  get allowEarlyData() {
    return this[kInternalClientState].allowEarlyData;
  }

  get handshakeStarted() {
    return this[kInternalClientState].handshakeStarted;
  }

  startHandshake() {
    const state = this[kInternalClientState];
    if (this.destroyed) {
      throw new ERR_INVALID_STATE(
        `${this.constructor.name} is already destroyed`);
    }
    if (state.handshakeStarted)
      return;
    state.handshakeStarted = true;
    this[kHandle].startHandshake();
  }

  get ephemeralKeyInfo() {
    return this[kHandle] !== undefined ?
      this[kHandle].getEphemeralKeyInfo() :
      {};
  }

  async setSocket(socket) {
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicClientSession is already destroyed');
    if (this.closing)
      throw new ERR_INVALID_STATE('QuicClientSession is closing');
    if (!(socket instanceof QuicSocket))
      throw new ERR_INVALID_ARG_TYPE('socket', 'QuicSocket', socket);
    if (socket.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket is already destroyed');
    if (socket.closing)
      throw new ERR_INVALID_STATE('QuicSocket is closing');

    await socket[kMaybeBind]();

    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicClientSession was destroyed');
    if (this.closing)
      throw new ERR_INVALID_STATE('QuicClientSession is closing');
    if (socket.destroyed)
      throw new ERR_INVALID_STATE('QuicSocket was destroyed');
    if (socket.closing)
      throw new ERR_INVALID_STATE('QuicSocket is closing');

    if (this.socket) {
      this.socket[kRemoveSession](this);
      this[kSetSocket](undefined);
    }
    socket[kAddSession](this);
    this[kSetSocket](socket);
  }
}

function streamOnResume() {
  if (!this.destroyed && this.readable)
    this[kHandle].readStart();
}

function streamOnPause() {
  if (!this.destroyed)
    this[kHandle].readStop();
}
class QuicStream extends Duplex {
  [kInternalState] = {
    closed: false,
    closePromise: undefined,
    closePromiseReject: undefined,
    closePromiseResolve: undefined,
    defaultEncoding: undefined,
    didRead: false,
    id: undefined,
    highWaterMark: undefined,
    push_id: undefined,
    resetCode: undefined,
    session: undefined,
    dataRateHistogram: undefined,
    dataSizeHistogram: undefined,
    dataAckHistogram: undefined,
    ready: false,
    sharedState: undefined,
    stats: undefined,
  };

  constructor(options, session, handle, push_id) {
    const {
      highWaterMark,
      defaultEncoding,
      readable = true,
      writable = true,
    } = options;

    super({
      readable,
      writable,
      highWaterMark,
      defaultEncoding,
      allowHalfOpen: true,
      decodeStrings: true,
      emitClose: true,
      autoDestroy: true,
      captureRejections: true,
    });
    const state = this[kInternalState];
    state.highWaterMark = highWaterMark;
    state.defaultEncoding = defaultEncoding;
    state.session = session;
    state.push_id = push_id;
    this._readableState.readingMore = true;
    this.on('pause', streamOnPause);

    if (handle !== undefined)
      this[kSetHandle](handle);
  }

  async _construct(callback) {
    try {
      if (this[kInternalState].ready)
        return callback();

      // Handle is already initialized
      const unidirectional = !this.readable;

      await this.session[kHandshakeComplete]();

      if (this.destroyed) {
        throw new ERR_INVALID_STATE('QuicStream was destroyed');
      }
      if (this.session.destroyed) {
        throw new ERR_INVALID_STATE(
          `${this.session.constructor.name} was destroyed`);
      }
      if (this.session.closing) {
        throw new ERR_INVALID_STATE(
          `${this.session.constructor.name} is closing`);
      }

      const handle =
        unidirectional ?
          _openUnidirectionalStream(this.session[kHandle]) :
          _openBidirectionalStream(this.session[kHandle]);

      if (handle === undefined)
        throw new ERR_OPERATION_FAILED('Unable to create QuicStream');

      this[kSetHandle](handle);
      callback();
    } catch (error) {
      callback(error);
    }
  }

  // Set handle is called once the QuicSession has been able
  // to complete creation of the internal QuicStream handle.
  // This will happen only after the QuicSession's own
  // internal handle has been created. The QuicStream object
  // is still minimally usable before this but any data
  // written will be buffered until kSetHandle is called.
  [kSetHandle](handle) {
    const state = this[kInternalState];
    const current = this[kHandle];
    this[kHandle] = handle;
    if (handle !== undefined) {
      handle.onread = onStreamRead;
      handle[owner_symbol] = this;
      this[async_id_symbol] = handle.getAsyncId();
      state.id = handle.id();
      state.dataRateHistogram = new Histogram(handle.rate);
      state.dataSizeHistogram = new Histogram(handle.size);
      state.dataAckHistogram = new Histogram(handle.ack);
      state.sharedState = new QuicStreamSharedState(handle.state);
      state.session[kAddStream](state.id, this);
      state.ready = true;
      this.emit(kReady);
    } else {
      if (current !== undefined) {
        current.stats[IDX_QUIC_STREAM_STATS_DESTROYED_AT] =
          process.hrtime.bigint();
        state.stats = new BigInt64Array(current.stats);
      }
      state.sharedState = undefined;
      if (state.dataRateHistogram)
        state.dataRateHistogram[kDestroyHistogram]();
      if (state.dataSizeHistogram)
        state.dataSizeHistogram[kDestroyHistogram]();
      if (state.dataAckHistogram)
        state.dataAckHistogram[kDestroyHistogram]();
    }
  }

  [kStreamReset](code) {
    // Receiving a reset from the peer indicates that it is no
    // longer sending any data, we can safely close the readable
    // side of the Duplex here.
    this[kInternalState].resetCode = code;
    this.push(null);
    this.read();
  }

  [kClose]() {
    const state = this[kInternalState];

    if (this.destroyed) {
      return PromiseReject(
        new ERR_INVALID_STATE('QuicStream is already destroyed'));
    }

    const promise = deferredClosePromise(state);
    if (this.readable) {
      this.push(null);
      this.read();
    }

    if (this.writable) {
      this.end();
    }

    // TODO(@jasnell): Investigate later if a Promise version
    // of finished() can work here instead.
    return promise;
  }

  close() {
    return this[kInternalState].closePromise || this[kClose]();
  }

  _destroy(error, callback) {
    const state = this[kInternalState];
    const handle = this[kHandle];
    this[kSetHandle]();
    if (handle !== undefined)
      handle.destroy();
    state.session[kRemoveStream](this);

    if (error && typeof state.closePromiseReject === 'function')
      state.closePromiseReject(error);
    else if (typeof state.closePromiseResolve === 'function')
      state.closePromiseResolve();

    // TODO(@jasnell): Investigate how we can eliminate the nextTick here
    process.nextTick(() => callback(error));
  }

  [kDestroy](code) {
    // TODO(@jasnell): If code is non-zero, and stream is not otherwise
    // naturally shutdown, then we should destroy with an error.

    // Put the QuicStream into detached mode before calling destroy
    this[kSetHandle]();
    this.destroy();
  }

  [kHeaders](headers, kind, push_id) {
    // TODO(@jasnell): Convert the headers into a proper object
    let name;
    switch (kind) {
      case QUICSTREAM_HEADERS_KIND_NONE:
        // Fall through
      case QUICSTREAM_HEADERS_KIND_INITIAL:
        name = 'initialHeaders';
        break;
      case QUICSTREAM_HEADERS_KIND_INFORMATIONAL:
        name = 'informationalHeaders';
        break;
      case QUICSTREAM_HEADERS_KIND_TRAILING:
        name = 'trailingHeaders';
        break;
      case QUICSTREAM_HEADERS_KIND_PUSH:
        name = 'pushHeaders';
        break;
      default:
        assert.fail('Invalid headers kind');
    }
    process.nextTick(emit.bind(this, name, headers, push_id));
  }

  [kAfterAsyncWrite]({ bytes }) {
    // There's currently nothing we need to do here. We have
    // to have this but it's a non-op
  }

  [kUpdateTimer]() {
    // Timeout is implemented in the QuicSession at the native
    // layer. We have to have this here but it's a non-op
  }

  [kTrackWriteState](stream, bytes) {
    // There's currently nothing to do here.
  }

  [kInspect](depth, options) {
    // TODO(@jasnell): Proper custom inspect implementation
    const direction = this.bidirectional ? 'bidirectional' : 'unidirectional';
    const initiated = this.serverInitiated ? 'server' : 'client';
    return customInspect(this, {
      id: this[kInternalState].id,
      detached: this.detached,
      direction,
      initiated,
      writableState: this._writableState,
      readableState: this._readableState
    }, depth, options);
  }

  get detached() {
    // The QuicStream is detached if it is yet destroyed
    // but the underlying handle is undefined. While in
    // detached mode, the QuicStream may still have
    // data pending in the read queue, but writes will
    // not be permitted.
    return this[kHandle] === undefined;
  }

  get serverInitiated() {
    return !!(this[kInternalState].id & 0b01);
  }

  get clientInitiated() {
    return !this.serverInitiated;
  }

  get unidirectional() {
    return !!(this[kInternalState].id & 0b10);
  }

  get bidirectional() {
    return !this.unidirectional;
  }

  [kWriteGeneric](writev, data, encoding, cb) {
    if (this.destroyed || this.detached)
      return;

    this[kUpdateTimer]();
    const req = (writev) ?
      writevGeneric(this, data, cb) :
      writeGeneric(this, data, encoding, cb);

    this[kTrackWriteState](this, req.bytes);
  }

  _write(data, encoding, cb) {
    this[kWriteGeneric](false, data, encoding, cb);
  }

  _writev(data, cb) {
    this[kWriteGeneric](true, data, '', cb);
  }

  // Called when the last chunk of data has been
  // acknowledged by the peer and end has been
  // called. By calling shutdown, we're telling
  // the native side that no more data will be
  // coming so that a fin stream packet can be
  // sent, allowing any remaining final stream
  // frames to be sent if necessary.
  //
  // When end() is called, we set the writeEnded
  // flag so that we can know earlier when there
  // is not going to be any more data being written
  // but that is only used when end() is called
  // with a final chunk to write.
  _final(cb) {
    if (!this.detached) {
      const state = this[kInternalState];
      if (state.sharedState?.finSent)
        return cb();
      const handle = this[kHandle];
      const req = new ShutdownWrap();
      req.oncomplete = () => {
        req.handle = undefined;
        cb();
      };
      req.handle = handle;
      if (handle.shutdown(req) === 1)
        return req.oncomplete();
      return;
    }
    return cb();
  }

  end(...args) {
    if (!this.destroyed) {
      if (!this.detached)
        this[kInternalState].sharedState.writeEnded = true;
      super.end.apply(this, args);
    }
    return this;
  }

  _read(nread) {
    if (this.destroyed) {
      this.push(null);
      return;
    }
    const state = this[kInternalState];
    if (!state.didRead) {
      this._readableState.readingMore = false;
      state.didRead = true;
    }

    streamOnResume.call(this);
  }

  sendFile(path, options = {}) {
    if (this.detached)
      throw new ERR_INVALID_STATE('Unable to send file');
    fs.open(path, 'r', QuicStream[kOnFileOpened].bind(this, options));
  }

  static [kOnFileOpened](options, err, fd) {
    const onError = options.onError;
    if (err) {
      if (onError) {
        this.close();
        onError(err);
      } else {
        this.destroy(err);
      }
      return;
    }

    if (this.destroyed || this.closed) {
      fs.close(fd, (err) => { if (err) throw err; });
      return;
    }

    this.sendFD(fd, options, true);
  }

  sendFD(fd, { offset = -1, length = -1 } = {}, ownsFd = false) {
    if (this.destroyed || this[kInternalState].closed)
      return;

    if (this.detached)
      throw new ERR_INVALID_STATE('Unable to send file descriptor');

    validateInteger(offset, 'options.offset', /* min */ -1);
    validateInteger(length, 'options.length', /* min */ -1);

    if (fd instanceof fsPromisesInternal.FileHandle)
      fd = fd.fd;
    else if (typeof fd !== 'number')
      throw new ERR_INVALID_ARG_TYPE('fd', ['number', 'FileHandle'], fd);

    this[kUpdateTimer]();
    this.ownsFd = ownsFd;

    defaultTriggerAsyncIdScope(this[async_id_symbol],
                               QuicStream[kStartFilePipe],
                               this, fd, offset, length);
  }

  static [kStartFilePipe](stream, fd, offset, length) {
    const handle = new FileHandle(fd, offset, length);
    handle.onread = QuicStream[kOnPipedFileHandleRead];
    handle.stream = stream;

    const pipe = new StreamPipe(handle, stream[kHandle]);
    pipe.onunpipe = QuicStream[kOnFileUnpipe];
    pipe.start();

    // Exact length of the file doesn't matter here, since the
    // stream is closing anyway - just use 1 to signify that
    // a write does exist
    stream[kTrackWriteState](stream, 1);
  }

  static [kOnFileUnpipe]() {  // Called on the StreamPipe instance.
    const stream = this.sink[owner_symbol];
    if (stream.ownsFd)
      this.source.close().catch(stream.destroy.bind(stream));
    else
      this.source.releaseFD();
    stream.end();
  }

  static [kOnPipedFileHandleRead]() {
    const err = streamBaseState[kReadBytesOrError];
    if (err < 0 && err !== UV_EOF) {
      this.stream.destroy(errnoException(err, 'sendFD'));
    }
  }

  get resetReceived() {
    const state = this[kInternalState];
    return (state.resetCode !== undefined) ?
      { code: state.resetCode | 0 } :
      undefined;
  }

  get id() {
    return this[kInternalState].id;
  }

  get push_id() {
    return this[kInternalState].push_id;
  }

  get session() {
    return this[kInternalState].session;
  }

  get dataRateHistogram() {
    return this[kInternalState].dataRateHistogram;
  }

  get dataSizeHistogram() {
    return this[kInternalState].dataSizeHistogram;
  }

  get dataAckHistogram() {
    return this[kInternalState].dataAckHistogram;
  }

  pushStream(headers = {}, options = {}) {
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicStream is already destroyed');

    if (this.detached) {
      throw new ERR_INVALID_STATE(
        'Push stream could not be opened on this QuicSession. ' +
        'Push is either disabled or currently blocked.');
    }

    const state = this[kInternalState];
    const {
      highWaterMark = state.highWaterMark,
      defaultEncoding = state.defaultEncoding,
    } = validateQuicStreamOptions(options);

    validateObject(headers, 'headers');

    // Push streams are only supported on QUIC servers, and
    // only if the original stream is bidirectional.
    // TODO(@jasnell): This is really an http/3 specific
    // requirement so if we end up later with another
    // QUIC application protocol that has a similar
    // notion of push streams without this restriction,
    // then we'll need to check alpn value here also.
    if (!this.clientInitiated && !this.bidirectional) {
      throw new ERR_INVALID_STATE(
        'Push streams are only supported on client-initiated, ' +
        'bidirectional streams');
    }

    // TODO(@jasnell): The assertValidPseudoHeader validator
    // here is HTTP/3 specific. If we end up with another
    // QUIC application protocol that supports push streams,
    // we will need to select a validator based on the
    // alpn value.
    const handle = this[kHandle].submitPush(
      mapToHeaders(headers, assertValidPseudoHeader));

    // If undefined is returned, it either means that push
    // streams are not supported by the underlying application,
    // or push streams are currently disabled/blocked. This
    // will typically be the case with HTTP/3 when the client
    // peer has disabled push.
    if (handle === undefined) {
      throw new ERR_INVALID_STATE(
        'Push stream could not be opened on this QuicSession. ' +
        'Push is either disabled or currently blocked.');
    }

    return new QuicStream({
      readable: false,
      highWaterMark,
      defaultEncoding,
    }, this.session, handle);
  }

  submitInformationalHeaders(headers = {}) {
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicStream is already destroyed');

    if (this.detached)
      throw new ERR_INVALID_STATE('Unable to submit headers');

    validateObject(headers, 'headers');

    // TODO(@jasnell): The validators here are specific to the QUIC
    // protocol. In the case below, these are the http/3 validators
    // (which are identical to the rules for http/2). We need to
    // find a way for this to be easily abstracted based on the
    // selected alpn.

    let validator;
    if (this.session instanceof QuicServerSession) {
      validator =
        this.clientInitiated ?
          assertValidPseudoHeaderResponse :
          assertValidPseudoHeader;
    } else {  // QuicClientSession
      validator =
        this.clientInitiated ?
          assertValidPseudoHeader :
          assertValidPseudoHeaderResponse;
    }

    return this[kHandle].submitInformationalHeaders(
      mapToHeaders(headers, validator));
  }

  submitInitialHeaders(headers = {}, options = {}) {
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicStream is already destroyed');

    if (this.detached)
      throw new ERR_INVALID_STATE('Unable to submit headers');

    const { terminal } = { ...options };

    if (terminal !== undefined)
      validateBoolean(terminal, 'options.terminal');
    validateObject(headers, 'headers');

    // TODO(@jasnell): The validators here are specific to the QUIC
    // protocol. In the case below, these are the http/3 validators
    // (which are identical to the rules for http/2). We need to
    // find a way for this to be easily abstracted based on the
    // selected alpn.

    let validator;
    if (this.session instanceof QuicServerSession) {
      validator =
        this.clientInitiated ?
          assertValidPseudoHeaderResponse :
          assertValidPseudoHeader;
    } else {  // QuicClientSession
      validator =
        this.clientInitiated ?
          assertValidPseudoHeader :
          assertValidPseudoHeaderResponse;
    }

    return this[kHandle].submitHeaders(
      mapToHeaders(headers, validator),
      terminal ?
        QUICSTREAM_HEADER_FLAGS_TERMINAL :
        QUICSTREAM_HEADER_FLAGS_NONE);
  }

  submitTrailingHeaders(headers = {}) {
    if (this.destroyed)
      throw new ERR_INVALID_STATE('QuicStream is already destroyed');

    if (this.detached)
      throw new ERR_INVALID_STATE('Unable to submit headers');

    validateObject(headers, 'headers');

    // TODO(@jasnell): The validators here are specific to the QUIC
    // protocol. In the case below, these are the http/3 validators
    // (which are identical to the rules for http/2). We need to
    // find a way for this to be easily abstracted based on the
    // selected alpn.

    return this[kHandle].submitTrailers(
      mapToHeaders(headers, assertValidPseudoHeaderTrailer));
  }

  get duration() {
    const end = getStats(this, IDX_QUIC_STREAM_STATS_DESTROYED_AT) ||
                process.hrtime.bigint();
    return Number(end - getStats(this, IDX_QUIC_STREAM_STATS_CREATED_AT));
  }

  get bytesReceived() {
    return Number(getStats(this, IDX_QUIC_STREAM_STATS_BYTES_RECEIVED));
  }

  get bytesSent() {
    return Number(getStats(this, IDX_QUIC_STREAM_STATS_BYTES_SENT));
  }

  get maxExtendedOffset() {
    return Number(getStats(this, IDX_QUIC_STREAM_STATS_MAX_OFFSET));
  }

  get finalSize() {
    return Number(getStats(this, IDX_QUIC_STREAM_STATS_FINAL_SIZE));
  }

  get maxAcknowledgedOffset() {
    return Number(getStats(this, IDX_QUIC_STREAM_STATS_MAX_OFFSET_ACK));
  }

  get maxReceivedOffset() {
    return Number(getStats(this, IDX_QUIC_STREAM_STATS_MAX_OFFSET_RECV));
  }
}

function createSocket(options) {
  return new QuicSocket(options);
}

module.exports = {
  createSocket,
  kUDPHandleForTesting,
  kRemoveFromSocket,
};

/* eslint-enable no-use-before-define */

// A single QuicSocket may act as both a Server and a Client.
// There are two kinds of sessions:
//   * QuicServerSession
//   * QuicClientSession
//
// It is important to understand that QUIC sessions are
// independent of the QuicSocket. A default configuration
// for QuicServerSession and QuicClientSessions may be
// set when the QuicSocket is created, but the actual
// configuration for a particular QuicSession instance is
// not set until the session itself is created.
//
// QuicSockets and QuicSession instances have distinct
// configuration options that apply independently:
//
// QuicSocket Options:
//   * `lookup` {Function} A function used to resolve DNS names.
//   * `type` {string} Either `'udp4'` or `'udp6'`, defaults to
//     `'udp4'`.
//   * `port` {number} The local IP port the QuicSocket will
//     bind to.
//   * `address` {string} The local IP address or hostname that
//     the QuicSocket will bind to. If a hostname is given, the
//     `lookup` function will be invoked to resolve an IP address.
//   * `ipv6Only`
//   * `reuseAddr`
//
// Keep in mind that while all QUIC network traffic is encrypted
// using TLS 1.3, every QuicSession maintains it's own SecureContext
// that is completely independent of the QuicSocket. Every
// QuicServerSession and QuicClientSession could, in theory,
// use a completely different TLS 1.3 configuration. To keep it
// simple, however, we use the same SecureContext for all QuicServerSession
// instances, but that may be something we want to revisit later.
//
// Every QuicSession has two sets of configuration parameters:
//   * Options
//   * Transport Parameters
//
// Options establish implementation specific operation parameters,
// such as the default highwatermark for new QuicStreams. Transport
// Parameters are QUIC specific and are passed to the peer as part
// of the TLS handshake.
//
// Every QuicSession may have separate options and transport
// parameters, even within the same QuicSocket, so the configuration
// must be established when the session is created.
//
// When creating a QuicSocket, it is possible to set a default
// configuration for both QuicServerSession and QuicClientSession
// options.
//
// const soc = createSocket({
//   type: 'udp4',
//   port: 0,
//   server: {
//     // QuicServerSession configuration defaults
//   },
//   client: {
//     // QuicClientSession configuration defaults
//   }
// });
//
// When calling listen() on the created QuicSocket, the server
// specific configuration that will be used for all new
// QuicServerSession instances will be given, with the values
// provided to createSocket() using the server option used
// as a default.
//
// When calling connect(), the client specific configuration
// will be given, with the values provided to the createSocket()
// using the client option used as a default.


// Some lifecycle documentation for the various objects:
//
// QuicSocket
//   Close
//     * Close all existing Sessions
//     * Do not allow any new Sessions (inbound or outbound)
//     * Destroy once there are no more sessions

//   Destroy
//     * Destroy all remaining sessions
//     * Destroy and free the QuicSocket handle immediately
//     * If Error, emit Error event
//     * Emit Close event

// QuicClientSession
//   Close
//     * Allow existing Streams to complete normally
//     * Do not allow any new Streams (inbound or outbound)
//     * Destroy once there are no more streams

//   Destroy
//     * Send CONNECTION_CLOSE
//     * Destroy all remaining Streams
//     * Remove Session from Parent Socket
//     * Destroy and free the QuicSession handle immediately
//     * If Error, emit Error event
//     * Emit Close event

// QuicServerSession
//   Close
//     * Allow existing Streams to complete normally
//     * Do not allow any new Streams (inbound or outbound)
//     * Destroy once there are no more streams
//   Destroy
//     * Send CONNECTION_CLOSE
//     * Destroy all remaining Streams
//     * Remove Session from Parent Socket
//     * Destroy and free the QuicSession handle immediately
//     * If Error, emit Error event
//     * Emit Close event

// QuicStream
//   Destroy
//     * Remove Stream From Parent Session
//     * Destroy and free the QuicStream handle immediately
//     * If Error, emit Error event
//     * Emit Close event

// QuicEndpoint States:
//   Initial                 -- Created, Endpoint not yet bound to local UDP
//                              port.
//   Pending                 -- Binding to local UDP port has been initialized.
//   Bound                   -- Binding to local UDP port has completed.
//   Closed                  -- Endpoint has been unbound from the local UDP
//                              port.
//   Error                   -- An error has been encountered, endpoint has
//                              been unbound and is no longer usable.
//
// QuicSocket States:
//   Initial                 -- Created, QuicSocket has at least one
//                              QuicEndpoint. All QuicEndpoints are in the
//                              Initial state.
//   Pending                 -- QuicSocket has at least one QuicEndpoint in the
//                              Pending state.
//   Bound                   -- QuicSocket has at least one QuicEndpoint in the
//                              Bound state.
//   Closed                  -- All QuicEndpoints are in the closed state.
//   Destroyed               -- QuicSocket is no longer usable
//   Destroyed-With-Error    -- An error has been encountered, socket is no
//                              longer usable
//
// QuicSession States:
//   Initial                 -- Created, QuicSocket state undetermined,
//                              no internal QuicSession Handle assigned.
//   Ready
//     QuicSocket Ready        -- QuicSocket in Bound state.
//     Handle Assigned         -- Internal QuicSession Handle assigned.
//   TLS Handshake Started   --
//   TLS Handshake Completed --
//   TLS Handshake Confirmed --
//   Closed                  -- Graceful Close Initiated
//   Destroyed               -- QuicSession is no longer usable
//   Destroyed-With-Error    -- An error has been encountered, session is no
//                              longer usable
//
// QuicStream States:
//   Initial Writable/Corked -- Created, QuicSession in Initial State
//   Open Writable/Uncorked  -- QuicSession in Ready State
//   Closed                  -- Graceful Close Initiated
//   Destroyed               -- QuicStream is no longer usable
//   Destroyed-With-Error    -- An error has been encountered, stream is no
//                              longer usable

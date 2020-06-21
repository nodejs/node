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
  RegExp,
  Set,
  Symbol,
} = primordials;

const { Buffer } = require('buffer');
const { isArrayBufferView } = require('internal/util/types');
const {
  getAllowUnauthorized,
  getSocketType,
  lookup4,
  lookup6,
  setTransportParams,
  toggleListeners,
  validateNumber,
  validateCloseCode,
  validateTransportParams,
  validateQuicClientSessionOptions,
  validateQuicSocketOptions,
  validateQuicStreamOptions,
  validateQuicSocketListenOptions,
  validateQuicEndpointOptions,
  validateCreateSecureContextOptions,
  validateQuicSocketConnectOptions,
} = require('internal/quic/util');
const util = require('util');
const assert = require('internal/assert');
const EventEmitter = require('events');
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
    ERR_INVALID_CALLBACK,
    ERR_QUIC_ERROR,
    ERR_QUICSESSION_DESTROYED,
    ERR_QUICSESSION_VERSION_NEGOTIATION,
    ERR_QUICSOCKET_DESTROYED,
    ERR_QUICSOCKET_LISTENING,
    ERR_QUICCLIENTSESSION_FAILED,
    ERR_QUICCLIENTSESSION_FAILED_SETSOCKET,
    ERR_QUICSESSION_UPDATEKEY,
    ERR_QUICSTREAM_DESTROYED,
    ERR_QUICSTREAM_INVALID_PUSH,
    ERR_QUICSTREAM_UNSUPPORTED_PUSH,
    ERR_QUICSTREAM_OPEN_FAILED,
    ERR_TLS_DH_PARAM_SIZE,
  },
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
    AF_INET,
    AF_INET6,
    IDX_QUIC_SESSION_MAX_PACKET_SIZE_DEFAULT,
    IDX_QUIC_SESSION_STATE_MAX_STREAMS_BIDI,
    IDX_QUIC_SESSION_STATE_MAX_STREAMS_UNI,
    IDX_QUIC_SESSION_STATE_MAX_DATA_LEFT,
    IDX_QUIC_SESSION_STATE_HANDSHAKE_CONFIRMED,
    IDX_QUIC_SESSION_STATE_IDLE_TIMEOUT,
    IDX_QUIC_SESSION_STATE_BYTES_IN_FLIGHT,
    IDX_QUIC_SESSION_STATS_CREATED_AT,
    IDX_QUIC_SESSION_STATS_HANDSHAKE_START_AT,
    IDX_QUIC_SESSION_STATS_BYTES_RECEIVED,
    IDX_QUIC_SESSION_STATS_BYTES_SENT,
    IDX_QUIC_SESSION_STATS_BIDI_STREAM_COUNT,
    IDX_QUIC_SESSION_STATS_UNI_STREAM_COUNT,
    IDX_QUIC_SESSION_STATS_STREAMS_IN_COUNT,
    IDX_QUIC_SESSION_STATS_STREAMS_OUT_COUNT,
    IDX_QUIC_SESSION_STATS_KEYUPDATE_COUNT,
    IDX_QUIC_SESSION_STATS_LOSS_RETRANSMIT_COUNT,
    IDX_QUIC_SESSION_STATS_ACK_DELAY_RETRANSMIT_COUNT,
    IDX_QUIC_SESSION_STATS_MAX_BYTES_IN_FLIGHT,
    IDX_QUIC_SESSION_STATS_BLOCK_COUNT,
    IDX_QUIC_SESSION_STATS_MIN_RTT,
    IDX_QUIC_SESSION_STATS_SMOOTHED_RTT,
    IDX_QUIC_SESSION_STATS_LATEST_RTT,
    IDX_QUIC_STREAM_STATS_CREATED_AT,
    IDX_QUIC_STREAM_STATS_BYTES_RECEIVED,
    IDX_QUIC_STREAM_STATS_BYTES_SENT,
    IDX_QUIC_STREAM_STATS_MAX_OFFSET,
    IDX_QUIC_STREAM_STATS_FINAL_SIZE,
    IDX_QUIC_STREAM_STATS_MAX_OFFSET_ACK,
    IDX_QUIC_STREAM_STATS_MAX_OFFSET_RECV,
    IDX_QUIC_SOCKET_STATS_CREATED_AT,
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
    QUICSOCKET_OPTIONS_VALIDATE_ADDRESS_LRU,
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
  validateString,
} = require('internal/validators');

const emit = EventEmitter.prototype.emit;

const kAddSession = Symbol('kAddSession');
const kAddStream = Symbol('kAddStream');
const kClose = Symbol('kClose');
const kCert = Symbol('kCert');
const kClientHello = Symbol('kClientHello');
const kContinueBind = Symbol('kContinueBind');
const kContinueConnect = Symbol('kContinueConnect');
const kDestroy = Symbol('kDestroy');
const kEndpointBound = Symbol('kEndpointBound');
const kEndpointClose = Symbol('kEndpointClose');
const kGetStreamOptions = Symbol('kGetStreamOptions');
const kHandshake = Symbol('kHandshake');
const kHandshakePost = Symbol('kHandshakePost');
const kHeaders = Symbol('kHeaders');
const kMaybeBind = Symbol('kMaybeBind');
const kSocketReady = Symbol('kSocketReady');
const kRemoveSession = Symbol('kRemove');
const kRemoveStream = Symbol('kRemoveStream');
const kServerBusy = Symbol('kServerBusy');
const kSetHandle = Symbol('kSetHandle');
const kSetSocket = Symbol('kSetSocket');
const kStreamClose = Symbol('kStreamClose');
const kStreamReset = Symbol('kStreamReset');
const kTrackWriteState = Symbol('kTrackWriteState');
const kUDPHandleForTesting = Symbol('kUDPHandleForTesting');
const kUsePreferredAddress = Symbol('kUsePreferredAddress');
const kVersionNegotiation = Symbol('kVersionNegotiation');

const kSocketUnbound = 0;
const kSocketPending = 1;
const kSocketBound = 2;
const kSocketClosing = 3;
const kSocketDestroyed = 4;

let diagnosticPacketLossWarned = false;
let warnedVerifyHostnameIdentity = false;

assert(process.versions.ngtcp2 !== undefined);

// Called by the C++ internals when the socket is closed.
// When this is called, the only thing left to do is destroy
// the QuicSocket instance.
function onSocketClose() {
  this[owner_symbol].destroy();
}

// Called by the C++ internals when an error occurs on the socket.
// When this is called, the only thing left to do is destroy
// the QuicSocket instance with an error.
// TODO(@jasnell): Should consolidate this with onSocketClose
function onSocketError(err) {
  this[owner_symbol].destroy(errnoException(err));
}

// Called by the C++ internals when the server busy state of
// the QuicSocket has been changed.
function onSocketServerBusy(on) {
  this[owner_symbol][kServerBusy](!!on);
}

// Called by the C++ internals when a new server QuicSession has been created.
function onSessionReady(handle) {
  const socket = this[owner_symbol];
  const session =
    new QuicServerSession(
      socket,
      handle,
      socket[kGetStreamOptions]());
  process.nextTick(emit.bind(socket, 'session', session));
}

// During an immediate close, all currently open QuicStreams are
// abruptly closed. If they are still writable or readable, an abort
// event will be emitted, and RESET_STREAM and STOP_SENDING frames
// will be transmitted as necessary. Once streams have been
// shutdown, a CONNECTION_CLOSE frame will be sent and the
// session will enter the closing period, after which it will
// be destroyed either when the idle timeout expires, the
// QuicSession is silently closed, or destroy is called.
function onSessionClose(code, family) {
  if (this[owner_symbol]) {
    this[owner_symbol][kClose](family, code);
  } else {
    // When there's no owner_symbol, the session was closed
    // before it could be fully set up. Just immediately
    // close everything down on the native side.
    this.destroy(code, family);
  }
}

// Called by the C++ internals when a QuicSession has been destroyed.
// When this is called, the QuicSession is no longer usable. Removing
// the handle and emitting close is the only action.
// TODO(@jasnell): In the future, this will need to act differently
// for QuicClientSessions when autoResume is enabled.
function onSessionDestroyed() {
  const session = this[owner_symbol];
  this[owner_symbol] = undefined;

  if (session) {
    session[kSetHandle]();
    process.nextTick(emit.bind(session, 'close'));
  }
}

// Used only within the onSessionClientHello function. Invoked
// to complete the client hello process.
function clientHelloCallback(err, ...args) {
  if (err) {
    this[owner_symbol].destroy(err);
    return;
  }
  try {
    this.onClientHelloDone(...args);
  } catch (err) {
    this[owner_symbol].destroy(err);
  }
}

// This callback is invoked at the start of the TLS handshake to provide
// some basic information about the ALPN, SNI, and Ciphers that are
// being requested. It is only called if the 'clientHello' event is
// listened for.
function onSessionClientHello(alpn, servername, ciphers) {
  this[owner_symbol][kClientHello](
    alpn,
    servername,
    ciphers,
    clientHelloCallback.bind(this));
}

// Used only within the onSessionCert function. Invoked
// to complete the session cert process.
function sessionCertCallback(err, context, ocspResponse) {
  if (err) {
    this[owner_symbol].destroy(err);
    return;
  }
  if (context != null && !context.context) {
    this[owner_symbol].destroy(
      new ERR_INVALID_ARG_TYPE(
        'context',
        'SecureContext',
        context));
  }
  if (ocspResponse != null) {
    if (typeof ocspResponse === 'string')
      ocspResponse = Buffer.from(ocspResponse);
    if (!isArrayBufferView(ocspResponse)) {
      this[owner_symbol].destroy(
        new ERR_INVALID_ARG_TYPE(
          'ocspResponse',
          ['string', 'Buffer', 'TypedArray', 'DataView'],
          ocspResponse));
    }
  }
  try {
    this.onCertDone(context ? context.context : undefined, ocspResponse);
  } catch (err) {
    this[owner_symbol].destroy(err);
  }
}

// This callback is only ever invoked for QuicServerSession instances,
// and is used to trigger OCSP request processing when needed. The
// user callback must invoke .onCertDone() in order for the
// TLS handshake to continue.
function onSessionCert(servername) {
  this[owner_symbol][kCert](servername, sessionCertCallback.bind(this));
}

// This callback is only ever invoked for QuicClientSession instances,
// and is used to deliver the OCSP response as provided by the server.
// If the requestOCSP configuration option is false, this will never
// be called.
function onSessionStatus(response) {
  this[owner_symbol][kCert](response);
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

// Called by the C++ internals to emit a QLog record.
function onSessionQlog(str) {
  if (this.qlogBuffer === undefined) this.qlogBuffer = '';

  const session = this[owner_symbol];

  if (session && session.listenerCount('qlog') > 0) {
    // Emit this chunk along with any previously buffered data.
    str = this.qlogBuffer + str;
    this.qlogBuffer = '';
    if (str === '') return;
    session.emit('qlog', str);
  } else {
    // Buffer the data until both the JS session object and a listener
    // become available.
    this.qlogBuffer += str;

    if (!session || this.waitingForQlogListener) return;
    this.waitingForQlogListener = true;

    function onNewListener(ev) {
      if (ev === 'qlog') {
        session.removeListener('newListener', onNewListener);
        process.nextTick(() => {
          onSessionQlog.call(this, '');
        });
      }
    }

    session.on('newListener', onNewListener);
  }
}

// Called when an error occurs in a QuicSession. When this happens,
// the only remedy is to destroy the session.
function onSessionError(error) {
  if (this[owner_symbol]) {
    this[owner_symbol].destroy(error);
  }
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

  // TODO(@jasnell): Get default options from session
  const uni = id & 0b10;
  const {
    highWaterMark,
    defaultEncoding,
  } = session[kGetStreamOptions]();
  const stream = new QuicStream({
    writable: !uni,
    highWaterMark,
    defaultEncoding,
  }, session, push_id);
  stream[kSetHandle](streamHandle);
  if (uni)
    stream.end();
  session[kAddStream](id, stream);
  process.nextTick(emit.bind(session, 'stream', stream));
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

// During a silent close, all currently open QuicStreams are abruptly
// closed. If they are still writable or readable, an abort event will be
// emitted, otherwise the stream is just destroyed. No RESET_STREAM or
// STOP_SENDING is transmitted to the peer.
function onSessionSilentClose(statelessReset, code, family) {
  this[owner_symbol][kDestroy](statelessReset, family, code);
}

// When a stream is flow control blocked, causes a blocked event
// to be emitted. This is a purely informational event.
function onStreamBlocked() {
  process.nextTick(emit.bind(this[owner_symbol], 'blocked'));
}

// Register the callbacks with the QUIC internal binding.
setCallbacks({
  onSocketClose,
  onSocketError,
  onSocketServerBusy,
  onSessionReady,
  onSessionCert,
  onSessionClientHello,
  onSessionClose,
  onSessionDestroyed,
  onSessionError,
  onSessionHandshake,
  onSessionKeylog,
  onSessionQlog,
  onSessionSilentClose,
  onSessionStatus,
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

// connectAfterLookup is invoked when the QuicSocket connect()
// method has been invoked. The first step is to resolve the given
// remote hostname into an ip address. Once resolution is complete,
// the resolved ip address needs to be passed on to the [kContinueConnect]
// function or the QuicClientSession needs to be destroyed.
function connectAfterLookup(type, err, ip) {
  if (err) {
    this.destroy(err);
    return;
  }
  this[kContinueConnect](type, ip);
}

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
  toggleListeners(this[kHandle], event, true);
}

function onRemoveListener(event) {
  toggleListeners(this[kHandle], event, false);
}

// QuicEndpoint wraps a UDP socket and is owned
// by a QuicSocket. It does not exist independently
// of the QuicSocket.
class QuicEndpoint {
  #state = kSocketUnbound;
  #socket = undefined;
  #udpSocket = undefined;
  #address = undefined;
  #ipv6Only = undefined;
  #lookup = undefined;
  #port = undefined;
  #reuseAddr = undefined;
  #type = undefined;
  #fd = undefined;

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
    this.#socket = socket;
    this.#address = address || (type === AF_INET6 ? '::' : '0.0.0.0');
    this.#ipv6Only = !!ipv6Only;
    this.#lookup = lookup || (type === AF_INET6 ? lookup6 : lookup4);
    this.#port = port;
    this.#reuseAddr = !!reuseAddr;
    this.#udpSocket = dgram.createSocket(type === AF_INET6 ? 'udp6' : 'udp4');

    // kUDPHandleForTesting is only used in the Node.js test suite to
    // artificially test the endpoint. This code path should never be
    // used in user code.
    if (typeof options[kUDPHandleForTesting] === 'object') {
      this.#udpSocket.bind(options[kUDPHandleForTesting]);
      this.#state = kSocketBound;
      this.#socket[kEndpointBound](this);
    }
    const udpHandle = this.#udpSocket[internalDgram.kStateSymbol].handle;
    const handle = new QuicEndpointHandle(socket[kHandle], udpHandle);
    handle[owner_symbol] = this;
    this[kHandle] = handle;
    socket[kHandle].addEndpoint(handle, !!preferred);
  }

  [kInspect]() {
    const obj = {
      address: this.address,
      fd: this.#fd,
      type: this.#type
    };
    return `QuicEndpoint ${util.format(obj)}`;
  }

  // afterLookup is invoked when binding a QuicEndpoint. The first
  // step to binding is to resolve the given hostname into an ip
  // address. Once resolution is complete, the ip address needs to
  // be passed on to the [kContinueBind] function or the QuicEndpoint
  // needs to be destroyed.
  static #afterLookup = function(err, ip) {
    if (err) {
      this.destroy(err);
      return;
    }
    this[kContinueBind](ip);
  };

  // kMaybeBind binds the endpoint on-demand if it is not already
  // bound. If it is bound, we return immediately, otherwise put
  // the endpoint into the pending state and initiate the binding
  // process by calling the lookup to resolve the IP address.
  [kMaybeBind]() {
    if (this.#state !== kSocketUnbound)
      return;
    this.#state = kSocketPending;
    this.#lookup(this.#address, QuicEndpoint.#afterLookup.bind(this));
  }

  // IP address resolution is completed and we're ready to finish
  // binding to the local port.
  [kContinueBind](ip) {
    const udpHandle = this.#udpSocket[internalDgram.kStateSymbol].handle;
    if (udpHandle == null) {
      // TODO(@jasnell): We may need to throw an error here. Under
      // what conditions does this happen?
      return;
    }
    const flags =
      (this.#reuseAddr ? UV_UDP_REUSEADDR : 0) |
      (this.#ipv6Only ? UV_UDP_IPV6ONLY : 0);

    const ret = udpHandle.bind(ip, this.#port, flags);
    if (ret) {
      this.destroy(exceptionWithHostPort(ret, 'bind', ip, this.#port || 0));
      return;
    }

    // On Windows, the fd will be meaningless, but we always record it.
    this.#fd = udpHandle.fd;
    this.#state = kSocketBound;

    // Notify the owning socket that the QuicEndpoint has been successfully
    // bound to the local UDP port.
    this.#socket[kEndpointBound](this);
  }

  [kDestroy](error) {
    const handle = this[kHandle];
    if (handle !== undefined) {
      this[kHandle] = undefined;
      handle[owner_symbol] = undefined;
      handle.ondone = () => {
        this.#udpSocket.close((err) => {
          if (err) error = err;
          this.#socket[kEndpointClose](this, error);
        });
      };
      handle.waitForPendingCallbacks();
    }
  }

  // If the QuicEndpoint is bound, returns an object detailing
  // the local IP address, port, and address type to which it
  // is bound. Otherwise, returns an empty object.
  get address() {
    if (this.#state !== kSocketDestroyed) {
      try {
        return this.#udpSocket.address();
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

  get fd() {
    return this.#fd;
  }

  // True if the QuicEndpoint has been destroyed and is
  // no longer usable.
  get destroyed() {
    return this.#state === kSocketDestroyed;
  }

  // True if binding has been initiated and is in progress.
  get pending() {
    return this.#state === kSocketPending;
  }

  // True if the QuicEndpoint has been bound to the local
  // UDP port.
  get bound() {
    return this.#state === kSocketBound;
  }

  setTTL(ttl) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('setTTL');
    this.#udpSocket.setTTL(ttl);
    return this;
  }

  setMulticastTTL(ttl) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('setMulticastTTL');
    this.#udpSocket.setMulticastTTL(ttl);
    return this;
  }

  setBroadcast(on = true) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('setBroadcast');
    this.#udpSocket.setBroadcast(on);
    return this;
  }

  setMulticastLoopback(on = true) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('setMulticastLoopback');
    this.#udpSocket.setMulticastLoopback(on);
    return this;
  }

  setMulticastInterface(iface) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('setMulticastInterface');
    this.#udpSocket.setMulticastInterface(iface);
    return this;
  }

  addMembership(address, iface) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('addMembership');
    this.#udpSocket.addMembership(address, iface);
    return this;
  }

  dropMembership(address, iface) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('dropMembership');
    this.#udpSocket.dropMembership(address, iface);
    return this;
  }

  ref() {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('ref');
    this.#udpSocket.ref();
    return this;
  }

  unref() {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('unref');
    this.#udpSocket.unref();
    return this;
  }

  destroy(error) {
    // If the QuicEndpoint is already destroyed, do nothing
    if (this.#state === kSocketDestroyed)
      return;

    // Mark the QuicEndpoint as being destroyed.
    this.#state = kSocketDestroyed;

    this[kDestroy](error);
  }
}

// QuicSocket wraps a UDP socket plus the associated TLS context and QUIC
// Protocol state. There may be *multiple* QUIC connections (QuicSession)
// associated with a single QuicSocket.
class QuicSocket extends EventEmitter {
  #alpn = undefined;
  #autoClose = undefined;
  #client = undefined;
  #defaultEncoding = undefined;
  #endpoints = new Set();
  #highWaterMark = undefined;
  #lookup = undefined;
  #server = undefined;
  #serverBusy = false;
  #serverListening = false;
  #serverSecureContext = undefined;
  #sessions = new Set();
  #state = kSocketUnbound;
  #stats = undefined;

  constructor(options) {
    const {
      endpoint,

      // True if the QuicSocket should automatically enter a graceful shutdown
      // if it is not listening as a server and the last QuicClientSession
      // closes
      autoClose,

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

      // UDP type
      type,

      // True if address verification should be used.
      validateAddress,

      // True if an LRU should be used for add validation
      validateAddressLRU,

      // Whether qlog should be enabled for sessions
      qlog,

      // Stateless reset token secret (16 byte buffer)
      statelessResetSecret,

      // When true, stateless resets will not be sent (default false)
      disableStatelessReset,
    } = validateQuicSocketOptions(options);
    super({ captureRejections: true });

    this.#autoClose = autoClose;
    this.#client = client;
    this.#lookup = lookup || (type === AF_INET6 ? lookup6 : lookup4);
    this.#server = server;

    const socketOptions =
      (validateAddress ? QUICSOCKET_OPTIONS_VALIDATE_ADDRESS : 0) |
      (validateAddressLRU ? QUICSOCKET_OPTIONS_VALIDATE_ADDRESS_LRU : 0);

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

    this.addEndpoint({
      lookup: this.#lookup,
      // Keep the lookup and ...endpoint in this order
      // to allow the passed in endpoint options to
      // override the lookup specifically for that endpoint
      ...endpoint,
      preferred: true
    });
  }

  // Returns the default QuicStream options for peer-initiated
  // streams. These are passed on to new client and server
  // QuicSession instances when they are created.
  [kGetStreamOptions]() {
    return {
      highWaterMark: this.#highWaterMark,
      defaultEncoding: this.#defaultEncoding,
    };
  }

  [kSetHandle](handle) {
    this[kHandle] = handle;
    if (handle !== undefined) {
      handle[owner_symbol] = this;
      this[async_id_symbol] = handle.getAsyncId();
    }
  }

  [kInspect]() {
    const obj = {
      endpoints: this.endpoints,
      sessions: this.#sessions,
    };
    return `QuicSocket ${util.format(obj)}`;
  }

  [kAddSession](session) {
    this.#sessions.add(session);
  }

  [kRemoveSession](session) {
    this.#sessions.delete(session);
    this[kMaybeDestroy]();
  }

  // Bind all QuicEndpoints on-demand, only if they haven't already been bound.
  // Function is a non-op if the socket is already bound or in the process of
  // being bound, and will call the callback once the socket is ready.
  [kMaybeBind](callback = () => {}) {
    if (this.#state === kSocketBound)
      return process.nextTick(callback);

    this.once('ready', callback);

    if (this.#state === kSocketPending)
      return;
    this.#state = kSocketPending;

    for (const endpoint of this.#endpoints)
      endpoint[kMaybeBind]();
  }

  [kEndpointBound](endpoint) {
    if (this.#state === kSocketBound)
      return;
    this.#state = kSocketBound;

    // Once the QuicSocket has been bound, we notify all currently
    // existing QuicSessions. QuicSessions created after this
    // point will automatically be notified that the QuicSocket
    // is ready.
    for (const session of this.#sessions)
      session[kSocketReady]();

    // The ready event indicates that the QuicSocket is ready to be
    // used to either listen or connect. No QuicServerSession should
    // exist before this event, and all QuicClientSession will remain
    // in Initial states until ready is invoked.
    process.nextTick(emit.bind(this, 'ready'));
  }

  // Called when a QuicEndpoint closes
  [kEndpointClose](endpoint, error) {
    this.#endpoints.delete(endpoint);
    process.nextTick(emit.bind(this, 'endpointClose', endpoint, error));

    // If there are no more QuicEndpoints, the QuicSocket is no
    // longer usable.
    if (this.#endpoints.size === 0) {
      // Ensure that there are absolutely no additional sessions
      for (const session of this.#sessions)
        session.destroy(error);

      if (error) process.nextTick(emit.bind(this, 'error', error));
      process.nextTick(emit.bind(this, 'close'));
    }
  }

  // kDestroy is called to actually free the QuicSocket resources and
  // cause the error and close events to be emitted.
  [kDestroy](error) {
    // The QuicSocket will be destroyed once all QuicEndpoints
    // are destroyed. See [kEndpointClose].
    for (const endpoint of this.#endpoints)
      endpoint.destroy(error);
  }

  // kMaybeDestroy is called one or more times after the close() method
  // is called. The QuicSocket will be destroyed if there are no remaining
  // open sessions.
  [kMaybeDestroy]() {
    if (this.closing && this.#sessions.size === 0) {
      this.destroy();
      return true;
    }
    return false;
  }

  // Called by the C++ internals to notify when server busy status is toggled.
  [kServerBusy](on) {
    this.#serverBusy = on;
    process.nextTick(emit.bind(this, 'busy', on));
  }

  addEndpoint(options = {}) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('listen');

    // TODO(@jasnell): Also forbid adding an endpoint if
    // the QuicSocket is closing.

    const endpoint = new QuicEndpoint(this, options);
    this.#endpoints.add(endpoint);

    // If the QuicSocket is already bound at this point,
    // also bind the newly created QuicEndpoint.
    if (this.#state !== kSocketUnbound)
      endpoint[kMaybeBind]();

    return endpoint;
  }

  // Used only from within the #continueListen function. When a preferred
  // address has been provided, the hostname given must be resolved into an
  // IP address, which must be passed on to #completeListen or the QuicSocket
  // needs to be destroyed.
  static #afterPreferredAddressLookup = function(
    transportParams,
    port,
    type,
    err,
    address) {
    if (err) {
      this.destroy(err);
      return;
    }
    this.#completeListen(transportParams, { address, port, type });
  };

  // The #completeListen function is called after all of the necessary
  // DNS lookups have been performed and we're ready to let the C++
  // internals begin listening for new QuicServerSession instances.
  #completeListen = function(transportParams, preferredAddress) {
    const {
      address,
      port,
      type = AF_INET,
    } = { ...preferredAddress };

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

    const options =
      (rejectUnauthorized ? QUICSERVERSESSION_OPTION_REJECT_UNAUTHORIZED : 0) |
      (requestCert ? QUICSERVERSESSION_OPTION_REQUEST_CERT : 0);

    // When the handle is told to listen, it will begin acting as a QUIC
    // server and will emit session events whenever a new QuicServerSession
    // is created.
    this[kHandle].listen(
      this.#serverSecureContext.context,
      address,
      type,
      port,
      this.#alpn,
      options);
    process.nextTick(emit.bind(this, 'listening'));
  };

  // When the QuicSocket listen() function is called, the first step is to bind
  // the underlying QuicEndpoint's. Once at least one endpoint has been bound,
  // the continueListen function is invoked to continue the process of starting
  // to listen.
  //
  // The preferredAddress is a preferred network endpoint that the server wishes
  // connecting clients to use. If specified, this will be communicate to the
  // client as part of the tranport parameters exchanged during the TLS
  // handshake.
  #continueListen = function(transportParams, lookup) {
    const { preferredAddress } = transportParams;

    // TODO(@jasnell): Currently, we wait to start resolving the
    // preferred address until after we know the QuicSocket is
    // bound. That's not strictly necessary as we can resolve the
    // preferred address in parallel, which out to be faster but
    // is a slightly more complicated to coordinate. In the future,
    // we should do those things in parallel.
    if (preferredAddress && typeof preferredAddress === 'object') {
      const {
        address,
        port,
        type = 'udp4',
      } = { ...preferredAddress };
      const typeVal = getSocketType(type);
      // If preferred address is set, we need to perform a lookup on it
      // to get the IP address. Only after that lookup completes can we
      // continue with the listen operation, passing in the resolved
      // preferred address.
      lookup(
        address || (typeVal === AF_INET6 ? '::' : '0.0.0.0'),
        QuicSocket.#afterPreferredAddressLookup.bind(
          this,
          transportParams,
          port,
          typeVal));
      return;
    }
    // If preferred address is not set, we can skip directly to the listen
    this.#completeListen(transportParams);
  };

  // Begin listening for server connections. The callback that may be
  // passed to this function is registered as a handler for the
  // on('session') event. Errors may be thrown synchronously by this
  // function.
  listen(options, callback) {
    if (this.#serverListening)
      throw new ERR_QUICSOCKET_LISTENING();

    if (this.#state === kSocketDestroyed ||
        this.#state === kSocketClosing) {
      throw new ERR_QUICSOCKET_DESTROYED('listen');
    }

    if (typeof options === 'function') {
      callback = options;
      options = {};
    }

    if (callback !== undefined && typeof callback !== 'function')
      throw new ERR_INVALID_CALLBACK(callback);

    options = {
      ...this.#server,
      ...options,
      minVersion: 'TLSv1.3',
      maxVersion: 'TLSv1.3',
    };

    // The ALPN protocol identifier is strictly required.
    const {
      alpn,
      defaultEncoding,
      highWaterMark,
      transportParams,
    } = validateQuicSocketListenOptions(options);

    // Store the secure context so that it is not garbage collected
    // while we still need to make use of it.
    this.#serverSecureContext =
      createSecureContext(
        options,
        initSecureContext);

    this.#highWaterMark = highWaterMark;
    this.#defaultEncoding = defaultEncoding;
    this.#serverListening = true;
    this.#alpn = alpn;

    // If the callback function is provided, it is registered as a
    // handler for the on('session') event and will be called whenever
    // there is a new QuicServerSession instance created.
    if (callback)
      this.on('session', callback);

    // Bind the QuicSocket to the local port if it hasn't been bound already.
    this[kMaybeBind](this.#continueListen.bind(
      this,
      transportParams,
      this.#lookup));
  }

  // When the QuicSocket connect() function is called, the first step is to bind
  // the underlying QuicEndpoint's. Once at least one endpoint has been bound,
  // the connectAfterBind function is invoked to continue the connection
  // process.
  //
  // The immediate next step is to resolve the address into an ip address.
  #continueConnect = function(session, lookup, address, type) {
    // TODO(@jasnell): Currently, we perform the DNS resolution after
    // the QuicSocket has been bound. We don't have to. We could do
    // it in parallel while we're waitint to be bound but doing so
    // is slightly more complicated.

    // Notice here that connectAfterLookup is bound to the QuicSession
    // that was created...
    lookup(
      address || (type === AF_INET6 ? '::' : '0.0.0.0'),
      connectAfterLookup.bind(session, type));
  };

  // Creates and returns a new QuicClientSession.
  connect(options, callback) {
    if (this.#state === kSocketDestroyed ||
      this.#state === kSocketClosing) {
      throw new ERR_QUICSOCKET_DESTROYED('connect');
    }

    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    options = {
      ...this.#client,
      ...options
    };

    const {
      type,
      address,
    } = validateQuicSocketConnectOptions(options);

    const session = new QuicClientSession(this, options);

    // TODO(@jasnell): This likely should listen for the secure event
    // rather than the ready event
    if (typeof callback === 'function')
      session.once('ready', callback);

    this[kMaybeBind](this.#continueConnect.bind(
      this,
      session,
      this.#lookup,
      address,
      type));

    return session;
  }

  // Initiate a Graceful Close of the QuicSocket.
  // Existing QuicClientSession and QuicServerSession instances will be
  // permitted to close naturally and gracefully on their own.
  // The QuicSocket will be immediately closed and freed as soon as there
  // are no additional session instances remaining. If there are no
  // QuicClientSession or QuicServerSession instances, the QuicSocket
  // will be immediately closed.
  //
  // If specified, the callback will be registered for once('close').
  //
  // No additional QuicServerSession instances will be accepted from
  // remote peers, and calls to connect() to create QuicClientSession
  // instances will fail. The QuicSocket will be otherwise usable in
  // every other way.
  //
  // Subsequent calls to close(callback) will register the close callback
  // if one is defined but will otherwise do nothing.
  //
  // Once initiated, a graceful close cannot be canceled. The graceful
  // close can be interupted, however, by abruptly destroying the
  // QuicSocket using the destroy() method.
  //
  // If close() is called before the QuicSocket has been bound (before
  // either connect() or listen() have been called, or the QuicSocket
  // is still in the pending state, the callback is registered for the
  // once('close') event (if specified) and the QuicSocket is destroyed
  // immediately.
  close(callback) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('close');

    // If a callback function is specified, it is registered as a
    // handler for the once('close') event. If the close occurs
    // immediately, the close event will be emitted as soon as the
    // process.nextTick queue is processed. Otherwise, the close
    // event will occur at some unspecified time in the near future.
    if (callback) {
      if (typeof callback !== 'function')
        throw new ERR_INVALID_CALLBACK();
      this.once('close', callback);
    }

    // If we are already closing, do nothing else and wait
    // for the close event to be invoked.
    if (this.#state === kSocketClosing)
      return;

    // If the QuicSocket is otherwise not bound to the local
    // port, destroy the QuicSocket immediately.
    if (this.#state !== kSocketBound) {
      this.destroy();
    }

    // Mark the QuicSocket as closing to prevent re-entry
    this.#state = kSocketClosing;

    // Otherwise, gracefully close each QuicSession, with
    // [kMaybeDestroy]() being called after each closes.
    const maybeDestroy = this[kMaybeDestroy].bind(this);

    // Tell the underlying QuicSocket C++ object to stop
    // listening for new QuicServerSession connections.
    // New initial connection packets for currently unknown
    // DCID's will be ignored.
    if (this[kHandle]) {
      this[kHandle].stopListening();
    }
    this.#serverListening = false;

    // If there are no sessions, calling maybeDestroy
    // will immediately and synchronously destroy the
    // QuicSocket.
    if (maybeDestroy())
      return;

    // If we got this far, there a QuicClientSession and
    // QuicServerSession instances still, we need to trigger
    // a graceful close for each of them. As each closes,
    // they will call the kMaybeDestroy function. When there
    // are no remaining session instances, the QuicSocket
    // will be closed and destroyed.
    for (const session of this.#sessions)
      session.close(maybeDestroy);
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
    // If the QuicSocket is already destroyed, do nothing
    if (this.#state === kSocketDestroyed)
      return;

    // Mark the QuicSocket as being destroyed.
    this.#state = kSocketDestroyed;

    // Immediately close any sessions that may be remaining.
    // If the udp socket is in a state where it is able to do so,
    // a final attempt to send CONNECTION_CLOSE frames for each
    // closed session will be made.
    for (const session of this.#sessions)
      session.destroy(error);

    this[kDestroy](error);
  }

  ref() {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('ref');
    for (const endpoint of this.#endpoints)
      endpoint.ref();
    return this;
  }

  unref() {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('unref');
    for (const endpoint of this.#endpoints)
      endpoint.unref();
    return this;
  }

  get endpoints() {
    return Array.from(this.#endpoints);
  }

  // The sever secure context is the SecureContext specified when calling
  // listen. It is the context that will be used with all new server
  // QuicSession instances.
  get serverSecureContext() {
    return this.#serverSecureContext;
  }

  // True if at least one associated QuicEndpoint has been successfully
  // bound to a local UDP port.
  get bound() {
    return this.#state === kSocketBound;
  }

  // True if graceful close has been initiated by calling close()
  get closing() {
    return this.#state === kSocketClosing;
  }

  // True if the QuicSocket has been destroyed and is no longer usable
  get destroyed() {
    return this.#state === kSocketDestroyed;
  }

  // True if listen() has been called successfully
  get listening() {
    return this.#serverListening;
  }

  // True if the QuicSocket is currently waiting on at least one
  // QuicEndpoint to succesfully bind.g
  get pending() {
    return this.#state === kSocketPending;
  }

  // Marking a server as busy will cause all new
  // connection attempts to fail with a SERVER_BUSY CONNECTION_CLOSE.
  setServerBusy(on = true) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('setServerBusy');
    validateBoolean(on, 'on');
    if (this.#serverBusy !== on)
      this[kHandle].setServerBusy(on);
  }

  get duration() {
    // TODO(@jasnell): If the object is destroyed, it should
    // use a fixed duration rather than calculating from now
    const now = process.hrtime.bigint();
    const stats = this.#stats || this[kHandle].stats;
    return now - stats[IDX_QUIC_SOCKET_STATS_CREATED_AT];
  }

  get boundDuration() {
    // TODO(@jasnell): If the object is destroyed, it should
    // use a fixed duration rather than calculating from now
    const now = process.hrtime.bigint();
    const stats = this.#stats || this[kHandle].stats;
    return now - stats[IDX_QUIC_SOCKET_STATS_BOUND_AT];
  }

  get listenDuration() {
    // TODO(@jasnell): If the object is destroyed, it should
    // use a fixed duration rather than calculating from now
    const now = process.hrtime.bigint();
    const stats = this.#stats || this[kHandle].stats;
    return now - stats[IDX_QUIC_SOCKET_STATS_LISTEN_AT];
  }

  get bytesReceived() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SOCKET_STATS_BYTES_RECEIVED];
  }

  get bytesSent() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SOCKET_STATS_BYTES_SENT];
  }

  get packetsReceived() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SOCKET_STATS_PACKETS_RECEIVED];
  }

  get packetsSent() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SOCKET_STATS_PACKETS_SENT];
  }

  get packetsIgnored() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SOCKET_STATS_PACKETS_IGNORED];
  }

  get serverBusy() {
    return this.#serverBusy;
  }

  get serverSessions() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SOCKET_STATS_SERVER_SESSIONS];
  }

  get clientSessions() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SOCKET_STATS_CLIENT_SESSIONS];
  }

  get statelessResetCount() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SOCKET_STATS_STATELESS_RESET_COUNT];
  }

  get serverBusyCount() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SOCKET_STATS_SERVER_BUSY_COUNT];
  }

  // Diagnostic packet loss is a testing mechanism that allows simulating
  // pseudo-random packet loss for rx or tx. The value specified for each
  // option is a number between 0 and 1 that identifies the possibility of
  // packet loss in the given direction.
  setDiagnosticPacketLoss(options) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('setDiagnosticPacketLoss');
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

  // Toggles stateless reset on/off. By default, stateless reset tokens
  // are generated when necessary. The disableStatelessReset option may
  // be used when the QuicSocket is created to disable generation of
  // stateless resets. The toggleStatelessReset method allows the setting
  // to be switched on/off dynamically through the lifetime of the
  // socket.
  toggleStatelessReset() {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('toggleStatelessReset');
    return this[kHandle].toggleStatelessReset();
  }
}

class QuicSession extends EventEmitter {
  #alpn = undefined;
  #cipher = undefined;
  #cipherVersion = undefined;
  #closeCode = NGTCP2_NO_ERROR;
  #closeFamily = QUIC_ERROR_APPLICATION;
  #closing = false;
  #destroyed = false;
  #earlyData = false;
  #handshakeComplete = false;
  #idleTimeout = false;
  #maxPacketLength = IDX_QUIC_SESSION_MAX_PACKET_SIZE_DEFAULT;
  #servername = undefined;
  #socket = undefined;
  #statelessReset = false;
  #stats = undefined;
  #pendingStreams = new Set();
  #streams = new Map();
  #verifyErrorReason = undefined;
  #verifyErrorCode = undefined;
  #handshakeAckHistogram = undefined;
  #handshakeContinuationHistogram = undefined;
  #highWaterMark = undefined;
  #defaultEncoding = undefined;

  constructor(socket, options) {
    const {
      alpn,
      servername,
      highWaterMark,
      defaultEncoding,
    } = options;
    super({ captureRejections: true });
    this.on('newListener', onNewListener);
    this.on('removeListener', onRemoveListener);
    this.#socket = socket;
    this.#servername = servername;
    this.#alpn = alpn;
    this.#highWaterMark = highWaterMark;
    this.#defaultEncoding = defaultEncoding;
    socket[kAddSession](this);
  }

  // kGetStreamOptions is called to get the configured options
  // for peer initiated QuicStream instances.
  [kGetStreamOptions]() {
    return {
      highWaterMark: this.#highWaterMark,
      defaultEncoding: this.#defaultEncoding,
    };
  }

  // Sets the internal handle for the QuicSession instance. For
  // server QuicSessions, this is called immediately as the
  // handle is created before the QuicServerSession JS object.
  // For client QuicSession instances, the connect() method
  // must first perform DNS resolution on the provided address
  // before the underlying QuicSession handle can be created.
  [kSetHandle](handle) {
    this[kHandle] = handle;
    if (handle !== undefined) {
      handle[owner_symbol] = this;
      this.#handshakeAckHistogram = new Histogram(handle.ack);
      this.#handshakeContinuationHistogram = new Histogram(handle.rate);
    } else {
      if (this.#handshakeAckHistogram)
        this.#handshakeAckHistogram[kDestroyHistogram]();
      if (this.#handshakeContinuationHistogram)
        this.#handshakeContinuationHistogram[kDestroyHistogram]();
    }
  }

  // Called when a client QuicSession instance receives a version
  // negotiation packet from the server peer. The client QuicSession
  // is destroyed immediately. This is not called at all for server
  // QuicSessions.
  [kVersionNegotiation](version, requestedVersions, supportedVersions) {
    const err =
      new ERR_QUICSESSION_VERSION_NEGOTIATION(
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

  // Causes the QuicSession to be immediately destroyed, but with
  // additional metadata set.
  [kDestroy](statelessReset, family, code) {
    this.#statelessReset = !!statelessReset;
    this.#closeCode = code;
    this.#closeFamily = family;
    this.destroy();
  }

  // Immediate close has been initiated for the session. Any
  // still open QuicStreams must be abandoned and shutdown
  // with RESET_STREAM and STOP_SENDING frames transmitted
  // as appropriate. Once the streams have been shutdown, a
  // CONNECTION_CLOSE will be generated and sent, switching
  // the session into the closing period.
  [kClose](family, code) {
    // Do nothing if the QuicSession has already been destroyed.
    if (this.#destroyed)
      return;

    // Set the close code and family so we can keep track.
    this.#closeCode = code;
    this.#closeFamily = family;

    // Shutdown all pending streams. These are Streams that
    // have been created but do not yet have a handle assigned.
    for (const stream of this.#pendingStreams)
      stream[kClose](family, code);

    // Shutdown all of the remaining streams
    for (const stream of this.#streams.values())
      stream[kClose](family, code);

    // By this point, all necessary RESET_STREAM and
    // STOP_SENDING frames ought to have been sent,
    // so now we just trigger sending of the
    // CONNECTION_CLOSE frame.
    this[kHandle].close(family, code);
  }

  // Closes the specified stream with the given code. The
  // QuicStream object will be destroyed.
  [kStreamClose](id, code) {
    const stream = this.#streams.get(id);
    if (stream === undefined)
      return;

    stream.destroy();
  }

  // Delivers a block of headers to the appropriate QuicStream
  // instance. This will only be called if the ALPN selected
  // is known to support headers.
  [kHeaders](id, headers, kind, push_id) {
    const stream = this.#streams.get(id);
    if (stream === undefined)
      return;

    stream[kHeaders](headers, kind, push_id);
  }

  [kStreamReset](id, code) {
    const stream = this.#streams.get(id);
    if (stream === undefined)
      return;

    stream[kStreamReset](code);
  }

  [kInspect]() {
    const obj = {
      alpn: this.#alpn,
      cipher: this.cipher,
      closing: this.closing,
      closeCode: this.closeCode,
      destroyed: this.destroyed,
      earlyData: this.#earlyData,
      maxStreams: this.maxStreams,
      servername: this.servername,
      streams: this.#streams.size,
      stats: {
        handshakeAck: this.handshakeAckHistogram,
        handshakeContinuation: this.handshakeContinuationHistogram,
      }
    };
    return `${this.constructor.name} ${util.format(obj)}`;
  }

  [kSetSocket](socket) {
    this.#socket = socket;
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
    this.#handshakeComplete = true;
    this.#servername = servername;
    this.#alpn = alpn;
    this.#cipher = cipher;
    this.#cipherVersion = cipherVersion;
    this.#maxPacketLength = maxPacketLength;
    this.#verifyErrorReason = verifyErrorReason;
    this.#verifyErrorCode = verifyErrorCode;
    this.#earlyData = earlyData;
    if (!this[kHandshakePost]())
      return;

    process.nextTick(
      emit.bind(this, 'secure', servername, alpn, this.cipher));
  }

  // Non-op for the default case. QuicClientSession
  // overrides this with some client-side specific
  // checks
  [kHandshakePost]() {
    return true;
  }

  [kRemoveStream](stream) {
    this.#streams.delete(stream.id);
  }

  [kAddStream](id, stream) {
    stream.once('close', this[kMaybeDestroy].bind(this));
    this.#streams.set(id, stream);
  }

  // The QuicSession will be destroyed if closing has been
  // called and there are no remaining streams
  [kMaybeDestroy]() {
    if (this.#closing && this.#streams.size === 0)
      this.destroy();
  }

  // Called when a client QuicSession has opted to use the
  // server provided preferred address. This is a purely
  // informationational notification. It is not called on
  // server QuicSession instances.
  [kUsePreferredAddress](address) {
    process.nextTick(
      emit.bind(this, 'usePreferredAddress', address));
  }

  // Closing allows any existing QuicStream's to complete
  // normally but disallows any new QuicStreams from being
  // opened. Calls to openStream() will fail, and new streams
  // from the peer will be rejected/ignored.
  close(callback) {
    if (this.#destroyed)
      throw new ERR_QUICSESSION_DESTROYED('close');

    if (callback) {
      if (typeof callback !== 'function')
        throw new ERR_INVALID_CALLBACK();
      this.once('close', callback);
    }

    // If we're already closing, do nothing else.
    // Callback will be invoked once the session
    // has been destroyed
    if (this.#closing)
      return;

    this.#closing = true;
    this[kHandle].gracefulClose();

    // See if we can close immediately.
    this[kMaybeDestroy]();
  }

  // Destroying synchronously shuts down and frees the
  // QuicSession immediately, even if there are still open
  // streams.
  //
  // A CONNECTION_CLOSE packet is sent to the
  // connected peer and the session is immediately
  // destroyed.
  //
  // If destroy is called with an error argument, the
  // 'error' event is emitted on next tick.
  //
  // Once destroyed, and after the 'error' event (if any),
  // the close event is emitted on next tick.
  destroy(error) {
    // Destroy can only be called once. Multiple calls will be ignored
    if (this.#destroyed)
      return;
    this.#destroyed = true;
    this.#closing = false;

    if (typeof error === 'number' ||
        (error != null &&
         typeof error === 'object' &&
         !(error instanceof Error))) {
      const {
        closeCode,
        closeFamily
      } = validateCloseCode(error);
      this.#closeCode = closeCode;
      this.#closeFamily = closeFamily;
      error = new ERR_QUIC_ERROR(closeCode, closeFamily);
    }

    // Destroy any pending streams immediately. These
    // are streams that have been created but have not
    // yet been assigned an internal handle.
    for (const stream of this.#pendingStreams)
      stream.destroy(error);

    // Destroy any remaining streams immediately.
    for (const stream of this.#streams.values())
      stream.destroy(error);

    this.removeListener('newListener', onNewListener);
    this.removeListener('removeListener', onRemoveListener);

    if (error) process.nextTick(emit.bind(this, 'error', error));

    const handle = this[kHandle];
    if (handle !== undefined) {
      // Copy the stats for use after destruction
      this.#stats = new BigInt64Array(handle.stats);
      this.#idleTimeout = !!handle.state[IDX_QUIC_SESSION_STATE_IDLE_TIMEOUT];
      // Calling destroy will cause a CONNECTION_CLOSE to be
      // sent to the peer and will destroy the QuicSession
      // handler immediately.
      handle.destroy(this.#closeCode, this.#closeFamily);
    } else {
      process.nextTick(emit.bind(this, 'close'));
    }

    // Remove the QuicSession JavaScript object from the
    // associated QuicSocket.
    this.#socket[kRemoveSession](this);
    this.#socket = undefined;
  }

  // For server QuicSession instances, true if earlyData is
  // enabled. For client QuicSessions, true only if session
  // resumption is used and early data was accepted during
  // the TLS handshake. The value is set only after the
  // TLS handshake is completed (immeditely before the
  // secure event is emitted)
  get usingEarlyData() {
    return this.#earlyData;
  }

  get maxStreams() {
    let bidi = 0;
    let uni = 0;
    if (this[kHandle]) {
      bidi = this[kHandle].state[IDX_QUIC_SESSION_STATE_MAX_STREAMS_BIDI];
      uni = this[kHandle].state[IDX_QUIC_SESSION_STATE_MAX_STREAMS_UNI];
    }
    return { bidi, uni };
  }

  get address() {
    return this.#socket ? this.#socket.address : {};
  }

  get maxDataLeft() {
    return this[kHandle] ?
      this[kHandle].state[IDX_QUIC_SESSION_STATE_MAX_DATA_LEFT] : 0;
  }

  get bytesInFlight() {
    return this[kHandle] ?
      this[kHandle].state[IDX_QUIC_SESSION_STATE_BYTES_IN_FLIGHT] : 0;
  }

  get blockCount() {
    return this[kHandle] ?
      this[kHandle].state[IDX_QUIC_SESSION_STATS_BLOCK_COUNT] : 0;
  }

  get authenticated() {
    // Specifically check for null. Undefined means the check has not
    // been performed yet, another other value other than null means
    // there was an error
    return this.#verifyErrorReason == null;
  }

  get authenticationError() {
    if (this.authenticated)
      return undefined;
    // eslint-disable-next-line no-restricted-syntax
    const err = new Error(this.#verifyErrorReason);
    const code = 'ERR_QUIC_VERIFY_' + this.#verifyErrorCode;
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
    return this.#handshakeComplete;
  }

  get handshakeConfirmed() {
    return Boolean(this[kHandle] ?
      this[kHandle].state[IDX_QUIC_SESSION_STATE_HANDSHAKE_CONFIRMED] : 0);
  }

  get idleTimeout() {
    return this.#idleTimeout;
  }

  get alpnProtocol() {
    return this.#alpn;
  }

  get cipher() {
    const name = this.#cipher;
    const version = this.#cipherVersion;
    return this.handshakeComplete ? { name, version } : {};
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
    if (!this[kHandle])
      throw new ERR_QUICSESSION_DESTROYED('ping');
    this[kHandle].ping();
  }

  get servername() {
    return this.#servername;
  }

  get destroyed() {
    return this.#destroyed;
  }

  get closing() {
    return this.#closing;
  }

  get closeCode() {
    return {
      code: this.#closeCode,
      family: this.#closeFamily
    };
  }

  get socket() {
    return this.#socket;
  }

  get statelessReset() {
    return this.#statelessReset;
  }

  openStream(options) {
    if (this.#destroyed || this.#closing)
      throw new ERR_QUICSESSION_DESTROYED('openStream');
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

    // TODO(@jasnell): This really shouldn't be necessary
    if (halfOpen) {
      stream.push(null);
      stream.read();
    }

    this.#pendingStreams.add(stream);

    // If early data is being used, we can create the internal QuicStream on the
    // ready event, that is immediately after the internal QuicSession handle
    // has been created. Otherwise, we have to wait until the secure event
    // signaling the completion of the TLS handshake.
    const makeStream = QuicSession.#makeStream.bind(this, stream, halfOpen);
    let deferred = false;
    if (this.allowEarlyData && !this.ready) {
      deferred = true;
      this.once('ready', makeStream);
    } else if (!this.handshakeComplete) {
      deferred = true;
      this.once('secure', makeStream);
    }

    if (!deferred)
      makeStream(stream, halfOpen);

    return stream;
  }

  static #makeStream = function(stream, halfOpen) {
    this.#pendingStreams.delete(stream);
    const handle =
      halfOpen ?
        _openUnidirectionalStream(this[kHandle]) :
        _openBidirectionalStream(this[kHandle]);

    if (handle === undefined) {
      stream.destroy(new ERR_QUICSTREAM_OPEN_FAILED());
      return;
    }

    stream[kSetHandle](handle);
    this[kAddStream](stream.id, stream);
  };

  get duration() {
    const now = process.hrtime.bigint();
    const stats = this.#stats || this[kHandle].stats;
    return now - stats[IDX_QUIC_SESSION_STATS_CREATED_AT];
  }

  get handshakeDuration() {
    const stats = this.#stats || this[kHandle].stats;
    const end =
      this.handshakeComplete ?
        stats[4] : process.hrtime.bigint();
    return end - stats[IDX_QUIC_SESSION_STATS_HANDSHAKE_START_AT];
  }

  get bytesReceived() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_BYTES_RECEIVED];
  }

  get bytesSent() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_BYTES_SENT];
  }

  get bidiStreamCount() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_BIDI_STREAM_COUNT];
  }

  get uniStreamCount() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_UNI_STREAM_COUNT];
  }

  get maxInFlightBytes() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_MAX_BYTES_IN_FLIGHT];
  }

  get lossRetransmitCount() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_LOSS_RETRANSMIT_COUNT];
  }

  get ackDelayRetransmitCount() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_ACK_DELAY_RETRANSMIT_COUNT];
  }

  get peerInitiatedStreamCount() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_STREAMS_IN_COUNT];
  }

  get selfInitiatedStreamCount() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_STREAMS_OUT_COUNT];
  }

  get keyUpdateCount() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_KEYUPDATE_COUNT];
  }

  get minRTT() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_MIN_RTT];
  }

  get latestRTT() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_LATEST_RTT];
  }

  get smoothedRTT() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_SESSION_STATS_SMOOTHED_RTT];
  }

  updateKey() {
    // Initiates a key update for the connection.
    if (this.#destroyed || this.#closing)
      throw new ERR_QUICSESSION_DESTROYED('updateKey');
    if (!this.handshakeConfirmed)
      throw new ERR_QUICSESSION_UPDATEKEY();
    return this[kHandle].updateKey();
  }

  get handshakeAckHistogram() {
    return this.#handshakeAckHistogram;
  }

  get handshakeContinuationHistogram() {
    return this.#handshakeContinuationHistogram;
  }

  // TODO(addaleax): This is a temporary solution for testing and should be
  // removed later.
  removeFromSocket() {
    return this[kHandle].removeFromSocket();
  }
}

class QuicServerSession extends QuicSession {
  #contexts = [];

  constructor(socket, handle, options) {
    const {
      highWaterMark,
      defaultEncoding,
    } = options;
    super(socket, { highWaterMark, defaultEncoding });
    this[kSetHandle](handle);

    // Both the handle and socket are immediately usable
    // at this point so trigger the ready event.
    this[kSocketReady]();
  }

  // Called only when a clientHello event handler is registered.
  // Allows user code an opportunity to interject into the start
  // of the TLS handshake.
  [kClientHello](alpn, servername, ciphers, callback) {
    this.emit(
      'clientHello',
      alpn,
      servername,
      ciphers,
      callback.bind(this[kHandle]));
  }

  // Called only when an OCSPRequest event handler is registered.
  // Allows user code the ability to answer the OCSP query.
  [kCert](servername, callback) {
    const { serverSecureContext } = this.socket;
    let { context } = serverSecureContext;

    for (var i = 0; i < this.#contexts.length; i++) {
      const elem = this.#contexts[i];
      if (elem[0].test(servername)) {
        context = elem[1];
        break;
      }
    }

    this.emit(
      'OCSPRequest',
      servername,
      context,
      callback.bind(this[kHandle]));
  }

  [kSocketReady]() {
    process.nextTick(emit.bind(this, 'ready'));
  }

  get ready() { return true; }

  get allowEarlyData() { return false; }

  addContext(servername, context = {}) {
    validateString(servername, 'servername');
    validateObject(context, 'context');

    // TODO(@jasnell): Consider unrolling regex
    const re = new RegExp('^' +
    servername.replace(/([.^$+?\-\\[\]{}])/g, '\\$1')
              .replace(/\*/g, '[^.]*') +
    '$');
    this.#contexts.push([re, _createSecureContext(context)]);
  }
}

class QuicClientSession extends QuicSession {
  #allowEarlyData = false;
  #autoStart = true;
  #dcid = undefined;
  #handshakeStarted = false;
  #ipv6Only = undefined;
  #minDHSize = undefined;
  #port = undefined;
  #ready = 0;
  #remoteTransportParams = undefined;
  #requestOCSP = undefined;
  #secureContext = undefined;
  #sessionTicket = undefined;
  #transportParams = undefined;
  #preferredAddressPolicy;
  #verifyHostnameIdentity = true;
  #qlogEnabled = false;

  constructor(socket, options) {
    const sc_options = {
      ...options,
      minVersion: 'TLSv1.3',
      maxVersion: 'TLSv1.3',
    };
    const {
      autoStart,
      alpn,
      dcid,
      ipv6Only,
      minDHSize,
      port,
      preferredAddressPolicy,
      remoteTransportParams,
      requestOCSP,
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

    super(socket, { servername, alpn, highWaterMark, defaultEncoding });
    this.#autoStart = autoStart;
    this.#handshakeStarted = autoStart;
    this.#dcid = dcid;
    this.#ipv6Only = ipv6Only;
    this.#minDHSize = minDHSize;
    this.#port = port || 0;
    this.#preferredAddressPolicy = preferredAddressPolicy;
    this.#requestOCSP = requestOCSP;
    this.#secureContext =
      createSecureContext(
        sc_options,
        initSecureContextClient);
    this.#transportParams = validateTransportParams(options);
    this.#verifyHostnameIdentity = verifyHostnameIdentity;
    this.#qlogEnabled = qlog;

    // If provided, indicates that the client is attempting to
    // resume a prior session. Early data would be enabled.
    this.#remoteTransportParams = remoteTransportParams;
    this.#sessionTicket = sessionTicket;
    this.#allowEarlyData =
      remoteTransportParams !== undefined &&
      sessionTicket !== undefined;

    if (socket.bound)
      this[kSocketReady]();
  }

  [kHandshakePost]() {
    const { type, size } = this.ephemeralKeyInfo;
    if (type === 'DH' && size < this.#minDHSize) {
      this.destroy(new ERR_TLS_DH_PARAM_SIZE(size));
      return false;
    }
    return true;
  }

  [kCert](response) {
    this.emit('OCSPResponse', response);
  }

  [kContinueConnect](type, ip) {
    setTransportParams(this.#transportParams);

    const options =
      (this.#verifyHostnameIdentity ?
        QUICCLIENTSESSION_OPTION_VERIFY_HOSTNAME_IDENTITY : 0) |
      (this.#requestOCSP ?
        QUICCLIENTSESSION_OPTION_REQUEST_OCSP : 0);

    const handle =
      _createClientSession(
        this.socket[kHandle],
        type,
        ip,
        this.#port,
        this.#secureContext.context,
        this.servername || ip,
        this.#remoteTransportParams,
        this.#sessionTicket,
        this.#dcid,
        this.#preferredAddressPolicy,
        this.alpnProtocol,
        options,
        this.#qlogEnabled,
        this.#autoStart);

    // We no longer need these, unset them so
    // memory can be garbage collected.
    this.#remoteTransportParams = undefined;
    this.#sessionTicket = undefined;
    this.#dcid = undefined;

    // If handle is a number, creating the session failed.
    if (typeof handle === 'number') {
      let reason;
      switch (handle) {
        case ERR_FAILED_TO_CREATE_SESSION:
          reason = 'QuicSession bootstrap failed';
          break;
        case ERR_INVALID_REMOTE_TRANSPORT_PARAMS:
          reason = 'Invalid Remote Transport Params';
          break;
        case ERR_INVALID_TLS_SESSION_TICKET:
          reason = 'Invalid TLS Session Ticket';
          break;
        default:
          reason = `${handle}`;
      }
      this.destroy(new ERR_QUICCLIENTSESSION_FAILED(reason));
      return;
    }

    this[kSetHandle](handle);

    // Listeners may have been added before the handle was created.
    // Ensure that we toggle those listeners in the handle state.

    if (this.listenerCount('keylog') > 0)
      toggleListeners(handle, 'keylog', true);

    if (this.listenerCount('pathValidation') > 0)
      toggleListeners(handle, 'pathValidation', true);

    if (this.listenerCount('usePreferredAddress') > 0)
      toggleListeners(handle, 'usePreferredAddress', true);

    this.#maybeReady(0x2);
  }

  [kSocketReady]() {
    this.#maybeReady(0x1);
  }

  // The QuicClientSession is ready for use only after
  // (a) The QuicSocket has been bound and
  // (b) The internal handle has been created
  #maybeReady = function(flag) {
    this.#ready |= flag;
    if (this.ready)
      process.nextTick(emit.bind(this, 'ready'));
  };

  get allowEarlyData() {
    return this.#allowEarlyData;
  }

  get ready() {
    return this.#ready === 0x3;
  }

  get handshakeStarted() {
    return this.#handshakeStarted;
  }

  startHandshake() {
    if (this.destroyed)
      throw new ERR_QUICSESSION_DESTROYED('startHandshake');
    if (this.#handshakeStarted)
      return;
    this.#handshakeStarted = true;
    if (!this.ready) {
      this.once('ready', () => this[kHandle].startHandshake());
    } else {
      this[kHandle].startHandshake();
    }
  }

  get ephemeralKeyInfo() {
    return this[kHandle] !== undefined ?
      this[kHandle].getEphemeralKeyInfo() :
      {};
  }

  #setSocketAfterBind = function(socket, callback) {
    if (socket.destroyed) {
      callback(new ERR_QUICSOCKET_DESTROYED('setSocket'));
      return;
    }

    if (!this[kHandle].setSocket(socket[kHandle])) {
      callback(new ERR_QUICCLIENTSESSION_FAILED_SETSOCKET());
      return;
    }

    if (this.socket) {
      this.socket[kRemoveSession](this);
      this[kSetSocket](undefined);
    }
    socket[kAddSession](this);
    this[kSetSocket](socket);

    callback();
  };

  setSocket(socket, callback) {
    if (!(socket instanceof QuicSocket))
      throw new ERR_INVALID_ARG_TYPE('socket', 'QuicSocket', socket);

    if (typeof callback !== 'function')
      throw new ERR_INVALID_CALLBACK();

    socket[kMaybeBind](() => this.#setSocketAfterBind(socket, callback));
  }
}

function streamOnResume() {
  if (!this.destroyed)
    this[kHandle].readStart();
}

function streamOnPause() {
  if (!this.destroyed /* && !this.pending */)
    this[kHandle].readStop();
}

class QuicStream extends Duplex {
  #closed = false;
  #aborted = false;
  #defaultEncoding = undefined;
  #didRead = false;
  #id = undefined;
  #highWaterMark = undefined;
  #push_id = undefined;
  #resetCode = undefined;
  #session = undefined;
  #dataRateHistogram = undefined;
  #dataSizeHistogram = undefined;
  #dataAckHistogram = undefined;
  #stats = undefined;

  constructor(options, session, push_id) {
    const {
      highWaterMark,
      defaultEncoding,
    } = options;

    super({
      highWaterMark,
      defaultEncoding,
      allowHalfOpen: true,
      decodeStrings: true,
      emitClose: true,
      autoDestroy: false,
      captureRejections: true,
    });
    this.#highWaterMark = highWaterMark;
    this.#defaultEncoding = defaultEncoding;
    this.#session = session;
    this.#push_id = push_id;
    this._readableState.readingMore = true;
    this.on('pause', streamOnPause);

    // The QuicStream writes are corked until kSetHandle
    // is set, ensuring that writes are buffered in JavaScript
    // until we have somewhere to send them.
    this.cork();
  }

  // Set handle is called once the QuicSession has been able
  // to complete creation of the internal QuicStream handle.
  // This will happen only after the QuicSession's own
  // internal handle has been created. The QuicStream object
  // is still minimally usable before this but any data
  // written will be buffered until kSetHandle is called.
  [kSetHandle](handle) {
    this[kHandle] = handle;
    if (handle !== undefined) {
      handle.onread = onStreamRead;
      handle[owner_symbol] = this;
      this[async_id_symbol] = handle.getAsyncId();
      this.#id = handle.id();
      this.#dataRateHistogram = new Histogram(handle.rate);
      this.#dataSizeHistogram = new Histogram(handle.size);
      this.#dataAckHistogram = new Histogram(handle.ack);
      this.uncork();
      this.emit('ready');
    } else {
      if (this.#dataRateHistogram)
        this.#dataRateHistogram[kDestroyHistogram]();
      if (this.#dataSizeHistogram)
        this.#dataSizeHistogram[kDestroyHistogram]();
      if (this.#dataAckHistogram)
        this.#dataAckHistogram[kDestroyHistogram]();
    }
  }

  [kStreamReset](code) {
    this.#resetCode = code | 0;
    this.push(null);
    this.read();
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

  [kClose](family, code) {
    // Trigger the abrupt shutdown of the stream. If the stream is
    // already no-longer readable or writable, this does nothing. If
    // the stream is readable or writable, then the abort event will
    // be emitted immediately after triggering the send of the
    // RESET_STREAM and STOP_SENDING frames. The stream will no longer
    // be readable or writable, but will not be immediately destroyed
    // as we need to wait until ngtcp2 recognizes the stream as
    // having been closed to be destroyed.

    // Do nothing if we've already been destroyed
    if (this.destroyed || this.#closed)
      return;

    if (this.pending)
      return this.once('ready', () => this[kClose](family, code));

    this.#closed = true;

    this.#aborted = this.readable || this.writable;

    // Trigger scheduling of the RESET_STREAM and STOP_SENDING frames
    // as appropriate. Notify ngtcp2 that the stream is to be shutdown.
    // Once sent, the stream will be closed and destroyed as soon as
    // the shutdown is acknowledged by the peer.
    this[kHandle].resetStream(code, family);

    // Close down the readable side of the stream
    if (this.readable) {
      this.push(null);
      this.read();
    }

    // It is important to call shutdown on the handle before shutting
    // down the writable side of the stream in order to prevent an
    // empty STREAM frame with fin set to be sent to the peer.
    if (this.writable)
      this.end();
  }

  [kAfterAsyncWrite]({ bytes }) {
    // TODO(@jasnell): Implement this
  }

  [kInspect]() {
    const direction = this.bidirectional ? 'bidirectional' : 'unidirectional';
    const initiated = this.serverInitiated ? 'server' : 'client';
    const obj = {
      id: this.#id,
      direction,
      initiated,
      writableState: this._writableState,
      readableState: this._readableState,
      stats: {
        dataRate: this.dataRateHistogram,
        dataSize: this.dataSizeHistogram,
        dataAck: this.dataAckHistogram,
      }
    };
    return `QuicStream ${util.format(obj)}`;
  }

  [kTrackWriteState](stream, bytes) {
    // TODO(@jasnell): Not yet sure what we want to do with these
    // this.#writeQueueSize += bytes;
    // this.#writeQueueSize += bytes;
    // this[kHandle].chunksSentSinceLastWrite = 0;
  }

  [kUpdateTimer]() {
    // TODO(@jasnell): Implement this later
  }

  get pending() {
    // The id is set in the kSetHandle function
    return this.#id === undefined;
  }

  get aborted() {
    return this.#aborted;
  }

  get serverInitiated() {
    return !!(this.#id & 0b01);
  }

  get clientInitiated() {
    return !this.serverInitiated;
  }

  get unidirectional() {
    return !!(this.#id & 0b10);
  }

  get bidirectional() {
    return !this.unidirectional;
  }

  #writeGeneric = function(writev, data, encoding, cb) {
    if (this.destroyed)
      return;  // TODO(addaleax): Can this happen?

    // The stream should be corked while still pending
    // but just in case uncork
    // was called early, defer the actual write until the
    // ready event is emitted.
    if (this.pending) {
      return this.once('ready', () => {
        this.#writeGeneric(writev, data, encoding, cb);
      });
    }

    this[kUpdateTimer]();
    const req = (writev) ?
      writevGeneric(this, data, cb) :
      writeGeneric(this, data, encoding, cb);

    this[kTrackWriteState](this, req.bytes);
  };

  _write(data, encoding, cb) {
    this.#writeGeneric(false, data, encoding, cb);
  }

  _writev(data, cb) {
    this.#writeGeneric(true, data, '', cb);
  }

  // Called when the last chunk of data has been
  // acknowledged by the peer and end has been
  // called. By calling shutdown, we're telling
  // the native side that no more data will be
  // coming so that a fin stream packet can be
  // sent.
  _final(cb) {
    // The QuicStream should be corked while pending
    // so this shouldn't be called, but just in case
    // the stream was prematurely uncorked, defer the
    // operation until the ready event is emitted.
    if (this.pending)
      return this.once('ready', () => this._final(cb));

    const handle = this[kHandle];
    if (handle === undefined) {
      cb();
      return;
    }

    const req = new ShutdownWrap();
    req.oncomplete = () => cb();
    req.handle = handle;
    const err = handle.shutdown(req);
    if (err === 1)
      return cb();
  }

  _read(nread) {
    if (this.pending)
      return this.once('ready', () => this._read(nread));

    if (this.destroyed) {  // TODO(addaleax): Can this happen?
      this.push(null);
      return;
    }
    if (!this.#didRead) {
      this._readableState.readingMore = false;
      this.#didRead = true;
    }

    streamOnResume.call(this);
  }

  sendFile(path, options = {}) {
    fs.open(path, 'r', QuicStream.#onFileOpened.bind(this, options));
  }

  static #onFileOpened = function(options, err, fd) {
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
  };

  sendFD(fd, { offset = -1, length = -1 } = {}, ownsFd = false) {
    if (this.destroyed || this.#closed)
      return;

    validateInteger(offset, 'options.offset', /* min */ -1);
    validateInteger(length, 'options.length', /* min */ -1);

    if (fd instanceof fsPromisesInternal.FileHandle)
      fd = fd.fd;
    else if (typeof fd !== 'number')
      throw new ERR_INVALID_ARG_TYPE('fd', ['number', 'FileHandle'], fd);

    if (this.pending) {
      return this.once('ready', () => {
        this.sendFD(fd, { offset, length }, ownsFd);
      });
    }

    this[kUpdateTimer]();
    this.ownsFd = ownsFd;

    // Close the writable side of the stream, but only as far as the writable
    // stream implementation is concerned.
    this._final = null;
    this.end();

    defaultTriggerAsyncIdScope(this[async_id_symbol],
                               QuicStream.#startFilePipe,
                               this, fd, offset, length);
  }

  static #startFilePipe = (stream, fd, offset, length) => {
    const handle = new FileHandle(fd, offset, length);
    handle.onread = QuicStream.#onPipedFileHandleRead;
    handle.stream = stream;

    const pipe = new StreamPipe(handle, stream[kHandle]);
    pipe.onunpipe = QuicStream.#onFileUnpipe;
    pipe.start();

    // Exact length of the file doesn't matter here, since the
    // stream is closing anyway - just use 1 to signify that
    // a write does exist
    stream[kTrackWriteState](stream, 1);
  }

  static #onFileUnpipe = function() {  // Called on the StreamPipe instance.
    const stream = this.sink[owner_symbol];
    if (stream.ownsFd)
      this.source.close().catch(stream.destroy.bind(stream));
    else
      this.source.releaseFD();
  };

  static #onPipedFileHandleRead = function() {
    const err = streamBaseState[kReadBytesOrError];
    if (err < 0 && err !== UV_EOF) {
      this.stream.destroy(errnoException(err, 'sendFD'));
    }
  };

  get resetReceived() {
    return (this.#resetCode !== undefined) ?
      { code: this.#resetCode | 0 } :
      undefined;
  }

  get bufferSize() {
    // TODO(@jasnell): Implement this
    return undefined;
  }

  get id() {
    return this.#id;
  }

  get push_id() {
    return this.#push_id;
  }

  close(code) {
    this[kClose](QUIC_ERROR_APPLICATION, code);
  }

  get session() {
    return this.#session;
  }

  _destroy(error, callback) {
    this.#session[kRemoveStream](this);
    const handle = this[kHandle];
    // Do not use handle after this point as the underlying C++
    // object has been destroyed. Any attempt to use the object
    // will segfault and crash the process.
    if (handle !== undefined) {
      this.#stats = new BigInt64Array(handle.stats);
      handle.destroy();
    }
    // The destroy callback must be invoked in a nextTick
    process.nextTick(() => callback(error));
  }

  _onTimeout() {
    // TODO(@jasnell): Implement this
  }

  get dataRateHistogram() {
    return this.#dataRateHistogram;
  }

  get dataSizeHistogram() {
    return this.#dataSizeHistogram;
  }

  get dataAckHistogram() {
    return this.#dataAckHistogram;
  }

  pushStream(headers = {}, options = {}) {
    if (this.destroyed)
      throw new ERR_QUICSTREAM_DESTROYED('push');

    const {
      highWaterMark = this.#highWaterMark,
      defaultEncoding = this.#defaultEncoding,
    } = validateQuicStreamOptions(options);

    validateObject(headers, 'headers');

    // Push streams are only supported on QUIC servers, and
    // only if the original stream is bidirectional.
    // TODO(@jasnell): This is really an http/3 specific
    // requirement so if we end up later with another
    // QUIC application protocol that has a similar
    // notion of push streams without this restriction,
    // then we'll need to check alpn value here also.
    if (!this.clientInitiated && !this.bidirectional)
      throw new ERR_QUICSTREAM_INVALID_PUSH();

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
    if (handle === undefined)
      throw new ERR_QUICSTREAM_UNSUPPORTED_PUSH();

    const stream = new QuicStream({
      readable: false,
      highWaterMark,
      defaultEncoding,
    }, this.session);

    // TODO(@jasnell): The null push and subsequent read shouldn't be necessary
    stream.push(null);
    stream.read();

    stream[kSetHandle](handle);
    this.session[kAddStream](stream.id, stream);
    return stream;
  }

  submitInformationalHeaders(headers = {}) {
    // TODO(@jasnell): Likely better to throw here instead of return false
    if (this.destroyed)
      return false;

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
    // TODO(@jasnell): Likely better to throw here instead of return false
    if (this.destroyed)
      return false;

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
    // TODO(@jasnell): Likely better to throw here instead of return false
    if (this.destroyed)
      return false;

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
    const now = process.hrtime.bigint();
    const stats = this.#stats || this[kHandle].stats;
    return now - stats[IDX_QUIC_STREAM_STATS_CREATED_AT];
  }

  get bytesReceived() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_STREAM_STATS_BYTES_RECEIVED];
  }

  get bytesSent() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_STREAM_STATS_BYTES_SENT];
  }

  get maxExtendedOffset() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_STREAM_STATS_MAX_OFFSET];
  }

  get finalSize() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_STREAM_STATS_FINAL_SIZE];
  }

  get maxAcknowledgedOffset() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_STREAM_STATS_MAX_OFFSET_ACK];
  }

  get maxReceivedOffset() {
    const stats = this.#stats || this[kHandle].stats;
    return stats[IDX_QUIC_STREAM_STATS_MAX_OFFSET_RECV];
  }
}

function createSocket(options) {
  return new QuicSocket(options);
}

module.exports = {
  createSocket,
  kUDPHandleForTesting
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

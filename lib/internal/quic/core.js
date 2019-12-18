'use strict';

/* eslint-disable no-use-before-define */

const {
  assertCrypto,
  customInspectSymbol: kInspect,
} = require('internal/util');

assertCrypto();

const { Symbol } = primordials;

const { Buffer } = require('buffer');
const { isArrayBufferView } = require('internal/util/types');
const {
  getAllowUnauthorized,
  getSocketType,
  lookup4,
  lookup6,
  setTransportParams,
  toggleListeners,
  validateCloseCode,
  validateTransportParams,
  validateQuicClientSessionOptions,
  validateQuicSocketOptions,
  validateQuicEndpointOptions,
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
    ERR_INVALID_OPT_VALUE,
    ERR_INVALID_CALLBACK,
    ERR_OUT_OF_RANGE,
    ERR_QUIC_ERROR,
    ERR_QUICSESSION_DESTROYED,
    ERR_QUICSESSION_VERSION_NEGOTIATION,
    ERR_QUICSOCKET_CLOSING,
    ERR_QUICSOCKET_DESTROYED,
    ERR_QUICSOCKET_LISTENING,
    ERR_QUICCLIENTSESSION_FAILED,
    ERR_QUICCLIENTSESSION_FAILED_SETSOCKET,
    ERR_QUICSESSION_UPDATEKEY,
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
    NGTCP2_ALPN_H3,
    NGTCP2_MAX_CIDLEN,
    NGTCP2_MIN_CIDLEN,
    IDX_QUIC_SESSION_MAX_PACKET_SIZE_DEFAULT,
    IDX_QUIC_SESSION_STATE_MAX_STREAMS_BIDI,
    IDX_QUIC_SESSION_STATE_MAX_STREAMS_UNI,
    IDX_QUIC_SESSION_STATE_MAX_DATA_LEFT,
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
    IDX_QUIC_STREAM_STATS_CREATED_AT,
    IDX_QUIC_STREAM_STATS_BYTES_RECEIVED,
    IDX_QUIC_STREAM_STATS_BYTES_SENT,
    IDX_QUIC_STREAM_STATS_MAX_OFFSET,
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
    QUICSTREAM_HEADER_FLAGS_NONE,
    QUICSTREAM_HEADER_FLAGS_TERMINAL,
  }
} = internalBinding('quic');

const {
  Histogram,
  kDestroy: kDestroyHistogram
} = require('internal/histogram');

const DEFAULT_QUIC_CIPHERS = 'TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384:' +
                             'TLS_CHACHA20_POLY1305_SHA256';
const DEFAULT_GROUPS = 'P-256:X25519:P-384:P-521';

const emit = EventEmitter.prototype.emit;

const kAddSession = Symbol('kAddSession');
const kAddStream = Symbol('kAddStream');
const kClose = Symbol('kClose');
const kCert = Symbol('kCert');
const kClientHello = Symbol('kClientHello');
const kContinueBind = Symbol('kContinueBind');
const kContinueConnect = Symbol('kContinueConnect');
const kContinueListen = Symbol('kContinueListen');
const kDestroy = Symbol('kDestroy');
const kEndpointBound = Symbol('kEndpointBound');
const kEndpointClose = Symbol('kEndpointClose');
const kHandshake = Symbol('kHandshake');
const kHandshakePost = Symbol('kHandshakePost');
const kHeaders = Symbol('kHeaders');
const kInit = Symbol('kInit');
const kMaybeBind = Symbol('kMaybeBind');
const kMaybeReady = Symbol('kMaybeReady');
const kReady = Symbol('kReady');
const kReceiveStart = Symbol('kReceiveStart');
const kReceiveStop = Symbol('kReceiveStop');
const kRemoveSession = Symbol('kRemove');
const kRemoveStream = Symbol('kRemoveStream');
const kServerBusy = Symbol('kServerBusy');
const kSetHandle = Symbol('kSetHandle');
const kSetSocket = Symbol('kSetSocket');
const kStreamClose = Symbol('kStreamClose');
const kStreamReset = Symbol('kStreamReset');
const kTrackWriteState = Symbol('kTrackWriteState');
const kUDPHandleForTesting = Symbol('kUDPHandleForTesting');
const kVersionNegotiation = Symbol('kVersionNegotiation');
const kWriteGeneric = Symbol('kWriteGeneric');

const kSocketUnbound = 0;
const kSocketPending = 1;
const kSocketBound = 2;
const kSocketClosing = 3;
const kSocketDestroyed = 4;

let diagnosticPacketLossWarned = false;

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
  const session = new QuicServerSession(socket, handle);
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
  this[owner_symbol][kClose](family, code);
}

// Called by the C++ internals when a QuicSession has been destroyed.
// When this is called, the QuicSession is no longer usable. Removing
// the handle and emitting close is the only action.
// TODO(@jasnell): In the future, this will need to act differently
// for QuicClientSessions when autoResume is enabled.
function onSessionDestroyed() {
  const session = this[owner_symbol];
  this[owner_symbol] = undefined;

  session[kSetHandle]();
  process.nextTick(emit.bind(session, 'close'));
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
  verifyErrorCode) {
  this[owner_symbol][kHandshake](
    servername,
    alpn,
    cipher,
    cipherVersion,
    maxPacketLength,
    verifyErrorReason,
    verifyErrorCode);
}

// Called by the C++ internals when TLS session ticket data is
// available. This is generally most useful on the client side
// where the session ticket needs to be persisted for session
// resumption and 0RTT.
function onSessionTicket(sessionID, sessionTicket, transportParams) {
  if (this[owner_symbol]) {
    process.nextTick(
      emit.bind(
        this[owner_symbol],
        'sessionTicket',
        sessionID,
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
function onStreamReady(streamHandle, id) {
  const session = this[owner_symbol];

  // onStreamReady should never be called if the stream is in a closing
  // state because new streams should not have been accepted at the C++
  // level.
  assert(!session.closing);

  // TODO(@jasnell): Get default options from session
  const uni = id & 0b10;
  const stream = new QuicStream({ writable: !uni }, session);
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
function onStreamReset(id, appErrorCode, finalSize) {
  this[owner_symbol][kStreamReset](id, appErrorCode, finalSize);
}

// Called when an error occurs in a QuicStream
function onStreamError(streamHandle, error) {
  streamHandle[owner_symbol].destroy(error);
}

// Called when a block of headers has been fully
// received for the stream. Not all QuicStreams
// will support headers. The headers argument
// here is an Array of name-value pairs.
function onStreamHeaders(id, headers, kind) {
  this[owner_symbol][kHeaders](id, headers, kind);
}

// During a silent close, all currently open QuicStreams are abruptly
// closed. If they are still writable or readable, an abort event will be
// emitted, otherwise the stream is just destroyed. No RESET_STREAM or
// STOP_SENDING is transmitted to the peer.
function onSessionSilentClose(statelessReset, code, family) {
  this[owner_symbol][kDestroy](statelessReset, family, code);
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
  onStreamHeaders,
});

// afterLookup is invoked when binding a QuicEndpoint. The first
// step to binding is to resolve the given hostname into an ip
// address. Once resolution is complete, the ip address needs to
// be passed on to the [kContinueBind] function or the QuicEndpoint
// needs to be destroyed.
function afterLookup(err, ip) {
  if (err) {
    this.destroy(err);
    return;
  }
  this[kContinueBind](ip);
}

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

// Used only from within the continueListen function. When a preferred address
// has been provided, the hostname given must be resolved into an IP address,
// which must be passed on to [kContinueListen] or the QuicSocket needs to be
// destroyed.
function afterPreferredAddressLookup(
  transportParams,
  port,
  type,
  err,
  address) {
  if (err) {
    this.destroy(err);
    return;
  }
  this[kContinueListen](transportParams, { address, port, type });
}

// When the QuicSocket listen() function is called, the first step is to bind
// the underlying QuicEndpoint's. Once at least one endpoint has been bound,
// the continueListen function is invoked to continue the process of starting
// to listen.
//
// The preferredAddress is a preferred network endpoint that the server wishes
// connecting clients to use. If specified, this will be communicate to the
// client as part of the tranport parameters exchanged during the TLS handshake.
function continueListen(transportParams, lookup) {
  const { preferredAddress } = transportParams;

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
      afterPreferredAddressLookup.bind(this, transportParams, port, typeVal));
    return;
  }
  // If preferred address is not set, we can skip directly to the listen
  this[kContinueListen](transportParams);
}

// When the QuicSocket connect() function is called, the first step is to bind
// the underlying QuicEndpoint's. Once at least one endpoint has been bound,
// the connectAfterBind function is invoked to continue the connection process.
//
// The immediate next step is to resolve the address into an ip address.
function connectAfterBind(session, lookup, address, type) {
  lookup(
    address || (type === AF_INET6 ? '::' : '0.0.0.0'),
    connectAfterLookup.bind(session, type));
}

// Creates the SecureContext used by QuicSocket instances that are listening
// for new connections.
function createSecureContext(options, init_cb) {
  const {
    ca,
    cert,
    ciphers = DEFAULT_QUIC_CIPHERS,
    clientCertEngine,
    crl,
    dhparam,
    ecdhCurve,
    groups = DEFAULT_GROUPS,
    honorCipherOrder,
    key,
    passphrase,
    pfx,
    sessionIdContext,
    secureProtocol
  } = { ...options };

  if (typeof ciphers !== 'string')
    throw new ERR_INVALID_ARG_TYPE('option.ciphers', 'string', ciphers);
  if (typeof groups !== 'string')
    throw new ERR_INVALID_ARG_TYPE('option.groups', 'string', groups);

  const sc = _createSecureContext({
    secureProtocol,
    ca,
    cert,
    ciphers: ciphers || DEFAULT_QUIC_CIPHERS,
    clientCertEngine,
    crl,
    dhparam,
    ecdhCurve,
    honorCipherOrder,
    key,
    passphrase,
    pfx,
    sessionIdContext
  });
  // Perform additional QUIC specific initialization on the SecureContext
  init_cb(sc.context, groups || DEFAULT_GROUPS);
  return sc;
}

function onNewListener(event) {
  toggleListeners(this, event, 1);
}

function onRemoveListener(event) {
  toggleListeners(this, event, 0);
}

// QuicEndpoint wraps a UDP socket.
class QuicEndpoint {
  #socket = undefined;
  #state = kSocketUnbound;
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
      port,
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

  [kMaybeBind]() {
    if (this.#state !== kSocketUnbound)
      return;
    this.#state = kSocketPending;
    this.#lookup(this.#address, afterLookup.bind(this));
  }

  [kContinueBind](ip) {
    const flags =
      (this.#reuseAddr ? UV_UDP_REUSEADDR : 0) |
      (this.#ipv6Only ? UV_UDP_IPV6ONLY : 0);
    const udpHandle = this.#udpSocket[internalDgram.kStateSymbol].handle;
    const ret = udpHandle.bind(ip, this.#port || 0, flags);
    if (ret) {
      this.destroy(exceptionWithHostPort(ret, 'bind', ip, this.#port || 0));
      return;
    }

    this.#state = kSocketBound;
    this.#fd = udpHandle.fd;
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

  get address() {
    const out = {};
    if (this.#state !== kSocketDestroyed) {
      try {
        return this.#udpSocket.address();
      } catch (err) {
        if (err.code === 'EBADF') {
          // If there is an EBADF error, the socket is not bound.
          // Return empty object
          return {};
        }
        throw err;
      }
    }
    return out;
  }

  get fd() {
    return this.#fd;
  }

  get closing() {
    return this.#state === kSocketClosing;
  }

  get destroyed() {
    return this.#state === kSocketDestroyed;
  }

  get pending() {
    return this.#state === kSocketPending;
  }

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
  #endpoints = new Set();
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

      // The maximum number of connections per host
      maxConnectionsPerHost,

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
      statelessResetSecret
    } = validateQuicSocketOptions(options);
    super();
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
        maxConnectionsPerHost,
        qlog,
        statelessResetSecret));

    this.addEndpoint({
      lookup: this.#lookup,
      // Keep the lookup and ...endpoint in this order
      // to allow the passed in endpoint options to
      // override the lookup specifically for that endpoint
      ...endpoint,
      preferred: true
    });
  }

  [kSetHandle](handle) {
    this[kHandle] = handle;
    if (handle) {
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
  }

  // Bind the UDP socket on demand, only if it hasn't already been bound.
  // Function is a non-op if the socket is already bound or in the process of
  // being bound, and will call the callback once the socket is ready.
  [kMaybeBind](callback = () => {}) {
    if (this.bound)
      return process.nextTick(callback);

    this.once('ready', callback);
    if (this.#state === kSocketPending)
      return;

    this.#state = kSocketPending;
    for (const endpoint of this.#endpoints)
      endpoint[kMaybeBind]();
  }

  // A socket should only be put into the receiving state if there is a
  // listening server or an active client. This will be called on demand
  // when needed.
  [kReceiveStart]() {
    this[kHandle].receiveStart();
  }

  // The socket should be moved to a not receiving state if there is no
  // listening server and no active sessions. This will be called on demand
  // when needed.
  [kReceiveStop]() {
    this[kHandle].receiveStop();
  }

  // The kContinueListen function is called after all of the necessary
  // DNS lookups have been performed and we're ready to let the C++
  // internals begin listening for new QuicServerSession instances.
  [kContinueListen](transportParams, preferredAddress) {
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
  }

  addEndpoint(options = {}) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('listen');
    const endpoint = new QuicEndpoint(this, options);
    this.#endpoints.add(endpoint);
    if (this.#state !== kSocketUnbound)
      endpoint[kMaybeBind]();
    return endpoint;
  }

  // Begin listening for server connections. The callback that may be
  // passed to this function is registered as a handler for the
  // on('session') event. Errors may be thrown synchronously by this
  // function.
  listen(options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = {};
    }

    if (callback && typeof callback !== 'function')
      throw new ERR_INVALID_CALLBACK();

    if (this.#serverListening)
      throw new ERR_QUICSOCKET_LISTENING();

    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('listen');

    if (this.#state === kSocketClosing)
      throw new ERR_QUICSOCKET_CLOSING('listen');

    // Bind the QuicSocket to the local port if it hasn't been bound already.
    this[kMaybeBind]();

    options = {
      secureProtocol: 'TLSv1_3_server_method',
      ...this.#server,
      ...options
    };

    const { alpn = NGTCP2_ALPN_H3 } = options;
    // The ALPN protocol identifier is strictly required.
    if (typeof alpn !== 'string')
      throw new ERR_INVALID_ARG_TYPE('options.alpn', 'string', alpn);

    // If the callback function is provided, it is registered as a
    // handler for the on('session') event and will be called whenever
    // there is a new QuicServerSession instance created.
    if (callback)
      this.on('session', callback);

    // Store the secure context so that it is not garbage collected
    // while we still need to make use of it.
    // TODO(@jasnell): We could store a reference at the C++ level instead
    // since we do not need to access this anywhere else.
    this.#serverSecureContext = createSecureContext(options, initSecureContext);
    this.#serverListening = true;
    this.#alpn = alpn;
    const doListen =
      continueListen.bind(
        this,
        validateTransportParams(options, NGTCP2_MAX_CIDLEN, NGTCP2_MIN_CIDLEN),
        this.#lookup);

    // If the QuicSocket is already bound, we'll begin listening
    // immediately. If we're still pending, however, wait until
    // the 'ready' event is emitted, then carry on.
    // TODO(@jasnell): Move the on ready handling to the kReady function
    // to avoid having to register the handler here.
    if (this.#state === kSocketPending) {
      this.once('ready', doListen);
      return;
    }
    doListen();
  }

  // Creates and returns a new QuicClientSession.
  connect(options, callback) {
    if (typeof options === 'function') {
      callback = options;
      options = undefined;
    }

    options = {
      ...this.#client,
      ...options
    };

    const {
      type = 'udp4',
      address,
    } = options;

    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('connect');

    if (this.#state === kSocketClosing)
      throw new ERR_QUICSOCKET_CLOSING('connect');

    const session = new QuicClientSession(this, options);

    // TODO(@jasnell): This likely should listen for the secure event
    // rather than the ready event
    if (typeof callback === 'function')
      session.once('ready', callback);

    this[kMaybeBind](connectAfterBind.bind(
      this,
      session,
      this.#lookup,
      address,
      getSocketType(type)));

    return session;
  }

  [kEndpointBound](endpoint) {
    if (this.#state === kSocketBound)
      return;
    this.#state = kSocketBound;
    for (const session of this.#sessions)
      session[kReady]();
    process.nextTick(emit.bind(this, 'ready'));
  }

  // Called when a QuicEndpoint closes
  [kEndpointClose](endpoint, error) {
    this.#endpoints.delete(endpoint);
    process.nextTick(emit.bind(this, 'endpointClose', endpoint, error));
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
    for (const endpoint of this.#endpoints)
      endpoint.destroy(error);
  }

  // kMaybeDestroy is called one or more times after the close() method
  // is called. The QuicSocket will be destroyed if there are no remaining
  // open sessions.
  [kMaybeDestroy]() {
    if (this.#state !== kSocketDestroyed && this.#sessions.size === 0) {
      this.destroy();
      return true;
    }
    return false;
  }

  [kServerBusy](on) {
    this.#serverBusy = on;
    process.nextTick(emit.bind(this, 'busy', on));
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

  get serverSecureContext() {
    return this.#serverSecureContext;
  }

  get closing() {
    return this.#state === kSocketClosing;
  }

  get destroyed() {
    return this.#state === kSocketDestroyed;
  }

  get pending() {
    return this.#state === kSocketPending;
  }

  // Marking a server as busy will cause all new
  // connection attempts to fail with a SERVER_BUSY CONNECTION_CLOSE.
  setServerBusy(on = true) {
    if (this.#state === kSocketDestroyed)
      throw new ERR_QUICSOCKET_DESTROYED('setServerBusy');
    if (typeof on !== 'boolean')
      throw new ERR_INVALID_ARG_TYPE('on', 'boolean', on);
    this[kHandle].setServerBusy(on);
  }

  get duration() {
    const now = process.hrtime.bigint();
    const stats = this.#stats || this[kHandle].stats;
    return now - stats[IDX_QUIC_SOCKET_STATS_CREATED_AT];
  }

  get boundDuration() {
    const now = process.hrtime.bigint();
    const stats = this.#stats || this[kHandle].stats;
    return now - stats[IDX_QUIC_SOCKET_STATS_BOUND_AT];
  }

  get listenDuration() {
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
    if (typeof rx !== 'number')
      throw new ERR_INVALID_ARG_TYPE('options.rx', 'number', rx);
    if (typeof tx !== 'number')
      throw new ERR_INVALID_ARG_TYPE('options.tx', 'number', rx);
    if (rx < 0.0 || rx > 1.0)
      throw new ERR_OUT_OF_RANGE('options.rx', '0.0 <= n <= 1.0', rx);
    if (tx < 0.0 || tx > 1.0)
      throw new ERR_OUT_OF_RANGE('options.tx', '0.0 <= n <= 1.0', tx);
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
  #alpn = undefined;
  #cipher = undefined;
  #cipherVersion = undefined;
  #closeCode = NGTCP2_NO_ERROR;
  #closeFamily = QUIC_ERROR_APPLICATION;
  #closing = false;
  #destroyed = false;
  #handshakeComplete = false;
  #maxPacketLength = IDX_QUIC_SESSION_MAX_PACKET_SIZE_DEFAULT;
  #recoveryStats = undefined;
  #servername = undefined;
  #socket = undefined;
  #statelessReset = false;
  #stats = undefined;
  #streams = new Map();
  #verifyErrorReason = undefined;
  #verifyErrorCode = undefined;
  #handshakeAckHistogram = undefined;
  #handshakeContinuationHistogram = undefined;

  constructor(socket, servername, alpn) {
    super();
    this.on('newListener', onNewListener);
    this.on('removeListener', onRemoveListener);
    this.#socket = socket;
    socket[kAddSession](this);
    this.#servername = servername;
    this.#alpn = alpn;
  }

  [kSetHandle](handle) {
    this[kHandle] = handle;
    if (handle !== undefined) {
      this.#handshakeAckHistogram =
        new Histogram(handle.crypto_rx_ack);
      this.#handshakeContinuationHistogram =
        new Histogram(handle.crypto_handshake_rate);
    } else {
      if (this.#handshakeAckHistogram)
        this.#handshakeAckHistogram[kDestroyHistogram]();
      if (this.#handshakeContinuationHistogram)
        this.#handshakeContinuationHistogram[kDestroyHistogram]();
    }
  }

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

  [kDestroy](statelessReset, family, code) {
    this.#statelessReset = !!statelessReset;
    this.#closeCode = code;
    this.#closeFamily = family;
    this.destroy();
  }

  [kClose](family, code) {
    // Immediate close has been initiated for the session. Any
    // still open QuicStreams must be abandoned and shutdown
    // with RESET_STREAM and STOP_SENDING frames transmitted
    // as appropriate. Once the streams have been shutdown, a
    // CONNECTION_CLOSE will be generated and sent, switching
    // the session into the closing period.

    // Do nothing if the QuicSession has already been destroyed.
    if (this.#destroyed)
      return;

    // Set the close code and family so we can keep track.
    this.#closeCode = code;
    this.#closeFamily = family;

    // Shutdown all of the remaining streams
    for (const stream of this.#streams.values())
      stream[kClose](family, code);

    // By this point, all necessary RESET_STREAM and
    // STOP_SENDING frames ought to have been sent,
    // so now we just trigger sending of the
    // CONNECTION_CLOSE frame.
    this[kHandle].close(family, code);
  }

  [kStreamClose](id, code) {
    const stream = this.#streams.get(id);
    if (stream === undefined)
      return;

    stream.destroy();
  }

  [kHeaders](id, headers, kind) {
    const stream = this.#streams.get(id);
    if (stream === undefined)
      return;

    stream[kHeaders](headers, kind);
  }

  [kStreamReset](id, code, finalSize) {
    const stream = this.#streams.get(id);
    if (stream === undefined)
      return;

    stream[kStreamReset](code, finalSize);
  }

  [kInspect]() {
    const obj = {
      alpn: this.#alpn,
      cipher: this.cipher,
      closing: this.closing,
      closeCode: this.closeCode,
      destroyed: this.destroyed,
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

  [kHandshake](
    servername,
    alpn,
    cipher,
    cipherVersion,
    maxPacketLength,
    verifyErrorReason,
    verifyErrorCode) {
    this.#handshakeComplete = true;
    this.#servername = servername;
    this.#alpn = alpn;
    this.#cipher = cipher;
    this.#cipherVersion = cipherVersion;
    this.#maxPacketLength = maxPacketLength;
    this.#verifyErrorReason = verifyErrorReason;
    this.#verifyErrorCode = verifyErrorCode;

    if (!this[kHandshakePost]())
      return;

    process.nextTick(
      emit.bind(this, 'secure', servername, alpn, this.cipher));
  }

  [kHandshakePost]() {
    // Non-op for the default case. QuicClientSession
    // overrides this with some client-side specific
    // checks
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

    // Destroy any remaining streams immediately.
    for (const stream of this.#streams.values())
      stream.destroy(error);

    this.removeListener('newListener', onNewListener);
    this.removeListener('removeListener', onRemoveListener);

    if (error) process.nextTick(emit.bind(this, 'error', error));

    const handle = this[kHandle];
    if (handle !== undefined) {
      // Copy the stats and recoveryStats for use after destruction
      this.#stats = new BigInt64Array(handle.stats);
      this.#recoveryStats = new Float64Array(handle.recoveryStats);
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
    return this.#verifyErrorReason === null;
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
      halfOpen = false,
      highWaterMark,
    } = { ...options };
    if (halfOpen !== undefined && typeof halfOpen !== 'boolean')
      throw new ERR_INVALID_ARG_TYPE('options.halfOpen', 'boolean', halfOpen);

    const stream = new QuicStream(
      {
        highWaterMark,
        readable: !halfOpen
      },
      this);
    if (halfOpen) {
      stream.push(null);
      stream.read();
    }

    if (!this.#handshakeComplete)
      this.once('secure', QuicSession.#makeStream.bind(this, stream, halfOpen));
    else
      QuicSession.#makeStream.call(this, stream, halfOpen);

    return stream;
  }

  static #makeStream = function(stream, halfOpen) {
    const handle =
      halfOpen ?
        _openUnidirectionalStream(this[kHandle]) :
        _openBidirectionalStream(this[kHandle]);
    if (handle === undefined)
      this.emit('error', new ERR_QUICSTREAM_OPEN_FAILED());

    stream[kSetHandle](handle);
    this[kAddStream](stream.id, stream);
  }

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
    const stats = this.#recoveryStats || this[kHandle].recoveryStats;
    return stats[0];
  }

  get latestRTT() {
    const stats = this.#recoveryStats || this[kHandle].recoveryStats;
    return stats[1];
  }

  get smoothedRTT() {
    const stats = this.#recoveryStats || this[kHandle].recoveryStats;
    return stats[2];
  }

  updateKey() {
    // Initiates a key update for the connection.
    if (this.#destroyed || this.#closing)
      throw new ERR_QUICSESSION_DESTROYED('updateKey');
    if (!this.handshakeComplete)
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

  constructor(socket, handle) {
    super(socket);
    this[kSetHandle](handle);
    handle[owner_symbol] = this;
  }

  [kClientHello](alpn, servername, ciphers, callback) {
    this.emit(
      'clientHello',
      alpn,
      servername,
      ciphers,
      callback.bind(this[kHandle]));
  }

  [kReady]() {
    process.nextTick(emit.bind(this, 'ready'));
  }

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

  addContext(servername, context = {}) {
    if (typeof servername !== 'string')
      throw new ERR_INVALID_ARG_TYPE('servername', 'string', servername);

    if (context == null || typeof context !== 'object')
      throw new ERR_INVALID_ARG_TYPE('context', 'Object', context);

    const re = new RegExp('^' +
    servername.replace(/([.^$+?\-\\[\]{}])/g, '\\$1')
              .replace(/\*/g, '[^.]*') +
    '$');
    this.#contexts.push([re, _createSecureContext(context)]);
  }
}

function setSocketAfterBind(socket, callback) {
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
}

let warnedVerifyHostnameIdentity;

class QuicClientSession extends QuicSession {
  #dcid = undefined;
  #handleReady = false;
  #ipv6Only = undefined;
  #minDHSize = undefined;
  #port = undefined;
  #remoteTransportParams = undefined;
  #requestOCSP = undefined;
  #secureContext = undefined;
  #sessionTicket = undefined;
  #socketReady = false;
  #transportParams = undefined;
  #preferredAddressPolicy;
  #verifyHostnameIdentity = true;
  #qlogEnabled = false;

  constructor(socket, options) {
    const sc_options = {
      secureProtocol: 'TLSv1_3_client_method',
      ...options
    };
    const {
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
    } = validateQuicClientSessionOptions(options);

    if (!verifyHostnameIdentity && !warnedVerifyHostnameIdentity) {
      warnedVerifyHostnameIdentity = true;
      process.emitWarning(
        'QUIC hostname identity verification is disabled. This violates QUIC ' +
        'specification requirements and reduces security. Hostname identity ' +
        'verification should only be disabled for debugging purposes.'
      );
    }

    super(socket, servername, alpn);
    this.#dcid = dcid;
    this.#ipv6Only = ipv6Only;
    this.#minDHSize = minDHSize;
    this.#port = port || 0;
    this.#preferredAddressPolicy = preferredAddressPolicy;
    this.#remoteTransportParams = remoteTransportParams;
    this.#requestOCSP = requestOCSP;
    this.#secureContext =
      createSecureContext(
        sc_options,
        initSecureContextClient);
    this.#sessionTicket = sessionTicket;
    this.#transportParams = validateTransportParams(options);
    this.#verifyHostnameIdentity = verifyHostnameIdentity;
    this.#qlogEnabled = qlog;
  }

  [kHandshakePost]() {
    const { type, size } = this.ephemeralKeyInfo;
    if (type === 'DH' && size < this.#minDHSize) {
      this.destroy(new ERR_TLS_DH_PARAM_SIZE(size));
      return false;
    }

    // TODO(@jasnell): QUIC *requires* that the client verify the
    // identity of the server so we'll need to do that here.
    // The current implementation of tls.checkServerIdentity is
    // less than great and could be rewritten to speed it up
    // significantly by running at the C++ layer. As it is
    // currently, the method pulls the peer cert data, converts
    // it to a javascript object, then processes the javascript
    // object... which is more expensive than what is strictly
    // necessary.
    //
    // See: _tls_wrap.js onConnectSecure function

    return true;
  }

  [kContinueConnect](type, ip) {
    const flags = this.#ipv6Only ? UV_UDP_IPV6ONLY : 0;
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
        flags,
        this.#secureContext.context,
        this.servername || ip,
        this.#remoteTransportParams,
        this.#sessionTicket,
        this.#dcid,
        this.#preferredAddressPolicy,
        this.alpnProtocol,
        options,
        this.#qlogEnabled);
    // We no longer need these, unset them so
    // memory can be garbage collected.
    this.#remoteTransportParams = undefined;
    this.#sessionTicket = undefined;
    this.#dcid = undefined;
    if (typeof handle === 'number') {
      let reason;
      switch (handle) {
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
    this[kInit](handle);
  }

  [kInit](handle) {
    this[kSetHandle](handle);
    handle[owner_symbol] = this;
    this.#handleReady = true;
    this[kMaybeReady]();
  }

  [kReady]() {
    this.#socketReady = true;
    this[kMaybeReady]();
  }

  [kCert](response) {
    this.emit('OCSPResponse', response);
  }

  [kMaybeReady]() {
    if (this.#socketReady && this.#handleReady)
      process.nextTick(emit.bind(this, 'ready'));
  }

  get ready() {
    return this.#handleReady && this.#socketReady;
  }

  get ephemeralKeyInfo() {
    return this[kHandle] !== undefined ?
      this[kHandle].getEphemeralKeyInfo() :
      {};
  }

  setSocket(socket, callback) {
    if (!(socket instanceof QuicSocket))
      throw new ERR_INVALID_ARG_TYPE('socket', 'QuicSocket', socket);

    if (typeof callback !== 'function')
      throw new ERR_INVALID_CALLBACK();

    socket[kMaybeBind](setSocketAfterBind.bind(this, socket, callback));
  }
}

function afterShutdown() {
  this.callback();
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
  #didRead = false;
  #id = undefined;
  #resetCode = undefined;
  #resetFinalSize = undefined;
  #session = undefined;
  #dataRateHistogram = undefined;
  #dataSizeHistogram = undefined;
  #dataAckHistogram = undefined;
  #stats = undefined;

  constructor(options, session) {
    super({
      ...options,
      allowHalfOpen: true,
      decodeStrings: true,
      emitClose: true
    });
    this.#session = session;
    this._readableState.readingMore = true;
    this.on('pause', streamOnPause);

    // See src/node_quic_stream.h for an explanation
    // of the initial states for unidirectional streams.
    if (this.unidirectional) {
      if (session instanceof QuicServerSession) {
        if (this.serverInitiated) {
          // Close the readable side
          this.push(null);
          this.read();
        } else {
          // Close the writable side
          this.end();
        }
      } else if (this.serverInitiated) {
        // Close the writable side
        this.end();
      } else {
        this.push(null);
        this.read();
      }
    }
  }

  [kSetHandle](handle) {
    this[kHandle] = handle;
    if (handle !== undefined) {
      handle.onread = onStreamRead;
      handle[owner_symbol] = this;
      this[async_id_symbol] = handle.getAsyncId();
      this.#id = handle.id();

      this.#dataRateHistogram = new Histogram(handle.data_rx_rate);
      this.#dataSizeHistogram = new Histogram(handle.data_rx_size);
      this.#dataAckHistogram = new Histogram(handle.data_rx_ack);

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

  [kStreamReset](code, finalSize) {
    this.#resetCode = code | 0;
    this.#resetFinalSize = finalSize | 0;
    this.push(null);
    this.read();
  }

  [kHeaders](headers, kind) {
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
    }
    process.nextTick(emit.bind(this, name, headers));
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

    // Finally, emit the abort event if necessary
    if (this.#aborted)
      process.nextTick(emit.bind(this, 'abort', code, family));
  }

  get pending() {
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

  [kWriteGeneric](writev, data, encoding, cb) {
    if (this.destroyed)
      return;  // TODO(addaleax): Can this happen?

    if (this.pending) {
      return this.once('ready', () => {
        this[kWriteGeneric](writev, data, encoding, cb);
      });
    }

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
  // sent.
  _final(cb) {
    if (this.pending)
      return this.once('ready', () => this._final(cb));

    const handle = this[kHandle];
    if (handle === undefined) {
      cb();
      return;
    }

    const req = new ShutdownWrap();
    req.oncomplete = afterShutdown;
    req.callback = cb;
    req.handle = handle;
    const err = handle.shutdown(req);
    if (err === 1)
      return afterShutdown.call(req, 0);
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
  }

  sendFD(fd, { offset = -1, length = -1 } = {}, ownsFd = false) {
    if (this.destroyed || this.#closed)
      return;

    if (typeof offset !== 'number')
      throw new ERR_INVALID_OPT_VALUE('options.offset', offset);
    if (typeof length !== 'number')
      throw new ERR_INVALID_OPT_VALUE('options.length', length);

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
  }

  static #onPipedFileHandleRead = function() {
    const err = streamBaseState[kReadBytesOrError];
    if (err < 0 && err !== UV_EOF) {
      this.stream.destroy(errnoException(err, 'sendFD'));
    }
  }

  get resetReceived() {
    return (this.#resetCode !== undefined) ?
      { code: this.#resetCode | 0, finalSize: this.#resetFinalSize | 0 } :
      undefined;
  }

  get bufferSize() {
    // TODO(@jasnell): Implement this
    return undefined;
  }

  get id() {
    return this.#id;
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
    callback(error);
  }

  _onTimeout() {
    // TODO(@jasnell): Implement this
  }

  [kUpdateTimer]() {
    // TODO(@jasnell): Implement this later
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

  submitInformationalHeaders(headers = {}) {
    // TODO(@jasnell): Likely better to throw here instead of return false
    if (this.destroyed)
      return false;

    if (!headers || typeof headers !== 'object')
      throw new ERR_INVALID_ARG_TYPE('headers', 'Object', headers);

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
    if (terminal !== undefined && typeof terminal !== 'boolean')
      throw new ERR_INVALID_ARG_TYPE('options.terminal', 'boolean', terminal);

    if (!headers || typeof headers !== 'object')
      throw new ERR_INVALID_ARG_TYPE('headers', 'Object', headers);

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

    if (!headers || typeof headers !== 'object')
      throw new ERR_INVALID_ARG_TYPE('headers', 'Object', headers);

    // TODO(@jasnell): The validators here are specific to the QUIC
    // protocol. In the case below, these are the http/3 validators
    // (which are identical to the rules for http/2). We need to
    // find a way for this to be easily abstracted based on the
    // selected alpn.

    return this[kHandle].submitTrailingHeaders(
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

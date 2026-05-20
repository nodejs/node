'use strict';

// TODO(@jasnell) Temporarily ignoring c8 covrerage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  ArrayBuffer,
  DataView,
  DataViewPrototypeGetBigInt64,
  DataViewPrototypeGetBigUint64,
  DataViewPrototypeGetByteLength,
  DataViewPrototypeGetUint16,
  DataViewPrototypeGetUint32,
  DataViewPrototypeGetUint8,
  DataViewPrototypeSetUint16,
  DataViewPrototypeSetUint32,
  DataViewPrototypeSetUint8,
  Float32Array,
  JSONStringify,
  Uint8Array,
} = primordials;

// Determine native byte order. The shared state buffer is written by
// C++ in native byte order, so DataView reads must match.
const kIsLittleEndian = (() => {
  // -1 as float32 is 0xBF800000. On little-endian, the bytes are
  // [0x00, 0x00, 0x80, 0xBF], so byte[3] is 0xBF (non-zero).
  // On big-endian, the bytes are [0xBF, 0x80, 0x00, 0x00], so byte[3] is 0.
  const buf = new Float32Array(1);
  buf[0] = -1;
  return new Uint8Array(buf.buffer)[3] !== 0;
})();

const {
  getOptionValue,
} = require('internal/options');

if (!process.features.quic || !getOptionValue('--experimental-quic')) {
  return;
}

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const {
  isArrayBuffer,
} = require('util/types');

const { inspect } = require('internal/util/inspect');
const assert = require('internal/assert');

const {
  kFinishClose,
  kInspect,
  kPrivateConstructor,
  kWantsHeaders,
  kWantsTrailers,
} = require('internal/quic/symbols');

// This file defines the helper objects for accessing state for
// various QUIC objects. Each of these wraps a DataView.
// Some of the state properties are read only, others are mutable.
// An ArrayBuffer is shared with the C++ level to allow for more
// efficient communication of state across the C++/JS boundary.
// When the state object is no longer needed, it is closed to
// prevent further updates to the buffer.

const {
  IDX_STATE_SESSION_LISTENER_FLAGS,
  IDX_STATE_SESSION_CLOSING,
  IDX_STATE_SESSION_GRACEFUL_CLOSE,
  IDX_STATE_SESSION_SILENT_CLOSE,
  IDX_STATE_SESSION_STATELESS_RESET,
  IDX_STATE_SESSION_HANDSHAKE_COMPLETED,
  IDX_STATE_SESSION_HANDSHAKE_CONFIRMED,
  IDX_STATE_SESSION_STREAM_OPEN_ALLOWED,
  IDX_STATE_SESSION_PRIORITY_SUPPORTED,
  IDX_STATE_SESSION_HEADERS_SUPPORTED,
  IDX_STATE_SESSION_WRAPPED,
  IDX_STATE_SESSION_APPLICATION_TYPE,
  IDX_STATE_SESSION_NO_ERROR_CODE,
  IDX_STATE_SESSION_INTERNAL_ERROR_CODE,
  IDX_STATE_SESSION_MAX_DATAGRAM_SIZE,
  IDX_STATE_SESSION_LAST_DATAGRAM_ID,
  IDX_STATE_SESSION_MAX_PENDING_DATAGRAMS,

  IDX_STATE_ENDPOINT_BOUND,
  IDX_STATE_ENDPOINT_RECEIVING,
  IDX_STATE_ENDPOINT_LISTENING,
  IDX_STATE_ENDPOINT_CLOSING,
  IDX_STATE_ENDPOINT_BUSY,
  IDX_STATE_ENDPOINT_MAX_CONNECTIONS_PER_HOST,
  IDX_STATE_ENDPOINT_MAX_CONNECTIONS_TOTAL,
  IDX_STATE_ENDPOINT_PENDING_CALLBACKS,

  IDX_STATE_STREAM_ID,
  IDX_STATE_STREAM_PENDING,
  IDX_STATE_STREAM_FIN_SENT,
  IDX_STATE_STREAM_FIN_RECEIVED,
  IDX_STATE_STREAM_READ_ENDED,
  IDX_STATE_STREAM_WRITE_ENDED,
  IDX_STATE_STREAM_RESET,
  IDX_STATE_STREAM_HAS_OUTBOUND,
  IDX_STATE_STREAM_HAS_READER,
  IDX_STATE_STREAM_WANTS_BLOCK,
  IDX_STATE_STREAM_WANTS_HEADERS,
  IDX_STATE_STREAM_WANTS_RESET,
  IDX_STATE_STREAM_WANTS_TRAILERS,
  IDX_STATE_STREAM_RECEIVED_EARLY_DATA,
  IDX_STATE_STREAM_WRITE_DESIRED_SIZE,
  IDX_STATE_STREAM_HIGH_WATER_MARK,
  IDX_STATE_STREAM_RESET_CODE,
} = internalBinding('quic');

assert(IDX_STATE_SESSION_LISTENER_FLAGS !== undefined);
assert(IDX_STATE_SESSION_CLOSING !== undefined);
assert(IDX_STATE_SESSION_GRACEFUL_CLOSE !== undefined);
assert(IDX_STATE_SESSION_SILENT_CLOSE !== undefined);
assert(IDX_STATE_SESSION_STATELESS_RESET !== undefined);
assert(IDX_STATE_SESSION_HANDSHAKE_COMPLETED !== undefined);
assert(IDX_STATE_SESSION_HANDSHAKE_CONFIRMED !== undefined);
assert(IDX_STATE_SESSION_STREAM_OPEN_ALLOWED !== undefined);
assert(IDX_STATE_SESSION_PRIORITY_SUPPORTED !== undefined);
assert(IDX_STATE_SESSION_HEADERS_SUPPORTED !== undefined);
assert(IDX_STATE_SESSION_WRAPPED !== undefined);
assert(IDX_STATE_SESSION_APPLICATION_TYPE !== undefined);
assert(IDX_STATE_SESSION_NO_ERROR_CODE !== undefined);
assert(IDX_STATE_SESSION_INTERNAL_ERROR_CODE !== undefined);
assert(IDX_STATE_SESSION_MAX_DATAGRAM_SIZE !== undefined);
assert(IDX_STATE_SESSION_LAST_DATAGRAM_ID !== undefined);
assert(IDX_STATE_ENDPOINT_BOUND !== undefined);
assert(IDX_STATE_ENDPOINT_RECEIVING !== undefined);
assert(IDX_STATE_ENDPOINT_LISTENING !== undefined);
assert(IDX_STATE_ENDPOINT_CLOSING !== undefined);
assert(IDX_STATE_ENDPOINT_BUSY !== undefined);
assert(IDX_STATE_ENDPOINT_PENDING_CALLBACKS !== undefined);
assert(IDX_STATE_STREAM_ID !== undefined);
assert(IDX_STATE_STREAM_PENDING !== undefined);
assert(IDX_STATE_STREAM_FIN_SENT !== undefined);
assert(IDX_STATE_STREAM_FIN_RECEIVED !== undefined);
assert(IDX_STATE_STREAM_READ_ENDED !== undefined);
assert(IDX_STATE_STREAM_WRITE_ENDED !== undefined);
assert(IDX_STATE_STREAM_RESET !== undefined);
assert(IDX_STATE_STREAM_HAS_OUTBOUND !== undefined);
assert(IDX_STATE_STREAM_HAS_READER !== undefined);
assert(IDX_STATE_STREAM_WANTS_BLOCK !== undefined);
assert(IDX_STATE_STREAM_WANTS_HEADERS !== undefined);
assert(IDX_STATE_STREAM_WANTS_RESET !== undefined);
assert(IDX_STATE_STREAM_WANTS_TRAILERS !== undefined);
assert(IDX_STATE_STREAM_WRITE_DESIRED_SIZE !== undefined);
assert(IDX_STATE_STREAM_RESET_CODE !== undefined);

class QuicEndpointState {
  /** @type {DataView} */
  #handle;

  /**
   * @param {symbol} privateSymbol
   * @param {ArrayBuffer} buffer
   */
  constructor(privateSymbol, buffer) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    if (!isArrayBuffer(buffer)) {
      throw new ERR_INVALID_ARG_TYPE('buffer', ['ArrayBuffer'], buffer);
    }
    this.#handle = new DataView(buffer);
  }

  /** @type {boolean} */
  get isBound() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_ENDPOINT_BOUND);
  }

  /** @type {boolean} */
  get isReceiving() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_ENDPOINT_RECEIVING);
  }

  /** @type {boolean} */
  get isListening() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_ENDPOINT_LISTENING);
  }

  /** @type {boolean} */
  get isClosing() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_ENDPOINT_CLOSING);
  }

  /** @type {boolean} */
  get isBusy() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_ENDPOINT_BUSY);
  }

  /** @type {number} */
  get maxConnectionsPerHost() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetUint16(
      this.#handle, IDX_STATE_ENDPOINT_MAX_CONNECTIONS_PER_HOST, kIsLittleEndian);
  }

  set maxConnectionsPerHost(val) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    DataViewPrototypeSetUint16(
      this.#handle, IDX_STATE_ENDPOINT_MAX_CONNECTIONS_PER_HOST, val, kIsLittleEndian);
  }

  /** @type {number} */
  get maxConnectionsTotal() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetUint16(
      this.#handle, IDX_STATE_ENDPOINT_MAX_CONNECTIONS_TOTAL, kIsLittleEndian);
  }

  set maxConnectionsTotal(val) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    DataViewPrototypeSetUint16(
      this.#handle, IDX_STATE_ENDPOINT_MAX_CONNECTIONS_TOTAL, val, kIsLittleEndian);
  }

  /**
   * The number of underlying callbacks that are pending. If the endpoint
   * is closing, these are the number of callbacks that the endpoint is
   * waiting on before it can be closed.
   * @type {bigint}
   */
  get pendingCallbacks() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetBigUint64(this.#handle, IDX_STATE_ENDPOINT_PENDING_CALLBACKS, kIsLittleEndian);
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return {};
    return {
      __proto__: null,
      isBound: this.isBound,
      isReceiving: this.isReceiving,
      isListening: this.isListening,
      isClosing: this.isClosing,
      isBusy: this.isBusy,
      maxConnectionsPerHost: this.maxConnectionsPerHost,
      maxConnectionsTotal: this.maxConnectionsTotal,
      pendingCallbacks: `${this.pendingCallbacks}`,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    if (DataViewPrototypeGetByteLength(this.#handle) === 0) {
      return 'QuicEndpointState { <Closed> }';
    }

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `QuicEndpointState ${inspect({
      isBound: this.isBound,
      isReceiving: this.isReceiving,
      isListening: this.isListening,
      isClosing: this.isClosing,
      isBusy: this.isBusy,
      pendingCallbacks: this.pendingCallbacks,
    }, opts)}`;
  }

  [kFinishClose]() {
    // Snapshot the state into a new DataView since the underlying
    // buffer will be destroyed.
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    this.#handle = new DataView(new ArrayBuffer(0));
  }
}

class QuicSessionState {
  /** @type {DataView} */
  #handle;

  /**
   * @param {symbol} privateSymbol
   * @param {ArrayBuffer} buffer
   */
  constructor(privateSymbol, buffer) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    if (!isArrayBuffer(buffer)) {
      throw new ERR_INVALID_ARG_TYPE('buffer', ['ArrayBuffer'], buffer);
    }
    this.#handle = new DataView(buffer);
  }

  // Listener flags are packed into a single uint32_t bitfield. The bit
  // positions must match the SessionListenerFlags enum in session.cc.
  static #LISTENER_PATH_VALIDATION = 1 << 0;
  static #LISTENER_DATAGRAM = 1 << 1;
  static #LISTENER_DATAGRAM_STATUS = 1 << 2;
  static #LISTENER_SESSION_TICKET = 1 << 3;
  static #LISTENER_NEW_TOKEN = 1 << 4;
  static #LISTENER_ORIGIN = 1 << 5;

  #getListenerFlag(flag) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!(DataViewPrototypeGetUint32(
      this.#handle, IDX_STATE_SESSION_LISTENER_FLAGS, kIsLittleEndian) & flag);
  }

  #setListenerFlag(flag, val) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    const current = DataViewPrototypeGetUint32(
      this.#handle, IDX_STATE_SESSION_LISTENER_FLAGS, kIsLittleEndian);
    DataViewPrototypeSetUint32(
      this.#handle, IDX_STATE_SESSION_LISTENER_FLAGS,
      val ? (current | flag) : (current & ~flag), kIsLittleEndian);
  }

  /** @type {boolean} */
  get hasPathValidationListener() {
    return this.#getListenerFlag(QuicSessionState.#LISTENER_PATH_VALIDATION);
  }
  set hasPathValidationListener(val) {
    this.#setListenerFlag(QuicSessionState.#LISTENER_PATH_VALIDATION, val);
  }

  /** @type {boolean} */
  get hasDatagramListener() {
    return this.#getListenerFlag(QuicSessionState.#LISTENER_DATAGRAM);
  }
  set hasDatagramListener(val) {
    this.#setListenerFlag(QuicSessionState.#LISTENER_DATAGRAM, val);
  }

  /** @type {boolean} */
  get hasDatagramStatusListener() {
    return this.#getListenerFlag(QuicSessionState.#LISTENER_DATAGRAM_STATUS);
  }
  set hasDatagramStatusListener(val) {
    this.#setListenerFlag(QuicSessionState.#LISTENER_DATAGRAM_STATUS, val);
  }

  /** @type {boolean} */
  get hasSessionTicketListener() {
    return this.#getListenerFlag(QuicSessionState.#LISTENER_SESSION_TICKET);
  }
  set hasSessionTicketListener(val) {
    this.#setListenerFlag(QuicSessionState.#LISTENER_SESSION_TICKET, val);
  }

  /** @type {boolean} */
  get hasNewTokenListener() {
    return this.#getListenerFlag(QuicSessionState.#LISTENER_NEW_TOKEN);
  }
  set hasNewTokenListener(val) {
    this.#setListenerFlag(QuicSessionState.#LISTENER_NEW_TOKEN, val);
  }

  /** @type {boolean} */
  get hasOriginListener() {
    return this.#getListenerFlag(QuicSessionState.#LISTENER_ORIGIN);
  }
  set hasOriginListener(val) {
    this.#setListenerFlag(QuicSessionState.#LISTENER_ORIGIN, val);
  }

  /** @type {boolean} */
  get isClosing() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_CLOSING);
  }

  /** @type {boolean} */
  get isGracefulClose() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_GRACEFUL_CLOSE);
  }

  /** @type {boolean} */
  get isSilentClose() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_SILENT_CLOSE);
  }

  /** @type {boolean} */
  get isStatelessReset() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_STATELESS_RESET);
  }

  /** @type {boolean} */
  get isHandshakeCompleted() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_HANDSHAKE_COMPLETED);
  }

  /** @type {boolean} */
  get isHandshakeConfirmed() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_HANDSHAKE_CONFIRMED);
  }

  /** @type {boolean} */
  get isStreamOpenAllowed() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_STREAM_OPEN_ALLOWED);
  }

  /** @type {boolean} */
  get isPrioritySupported() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_PRIORITY_SUPPORTED);
  }

  /**
   * Whether the negotiated application protocol supports headers.
   * Returns 0 (unknown), 1 (supported), or 2 (not supported).
   * @type {number}
   */
  get headersSupported() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_HEADERS_SUPPORTED);
  }

  /** @type {boolean} */
  get isWrapped() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_WRAPPED);
  }

  /** @type {number} */
  get applicationType() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_APPLICATION_TYPE);
  }

  /**
   * The negotiated application protocol's "no error" code, populated
   * by the C++ layer when the application is selected during ALPN
   * negotiation. For raw QUIC this is `0n`; for HTTP/3 this is
   * `0x100n` (`H3_NO_ERROR`).
   * @type {bigint}
   */
  get noErrorCode() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetBigUint64(
      this.#handle, IDX_STATE_SESSION_NO_ERROR_CODE, kIsLittleEndian);
  }

  /**
   * The negotiated application protocol's "internal error" code,
   * populated by the C++ layer when the application is selected
   * during ALPN negotiation. Used as the wire code for `RESET_STREAM`
   * frames when a stream is aborted without a more specific code.
   * For raw QUIC this is `0x1n` (NGTCP2_INTERNAL_ERROR); for HTTP/3
   * this is `0x102n` (`H3_INTERNAL_ERROR`).
   * @type {bigint}
   */
  get internalErrorCode() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetBigUint64(
      this.#handle, IDX_STATE_SESSION_INTERNAL_ERROR_CODE, kIsLittleEndian);
  }

  /** @type {number} */
  get maxDatagramSize() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetUint16(this.#handle, IDX_STATE_SESSION_MAX_DATAGRAM_SIZE, kIsLittleEndian);
  }

  /** @type {bigint} */
  get lastDatagramId() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetBigUint64(this.#handle, IDX_STATE_SESSION_LAST_DATAGRAM_ID, kIsLittleEndian);
  }

  /** @type {number} */
  get maxPendingDatagrams() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetUint16(
      this.#handle, IDX_STATE_SESSION_MAX_PENDING_DATAGRAMS, kIsLittleEndian);
  }

  set maxPendingDatagrams(val) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    DataViewPrototypeSetUint16(
      this.#handle, IDX_STATE_SESSION_MAX_PENDING_DATAGRAMS, val, kIsLittleEndian);
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return {};
    return {
      __proto__: null,
      hasPathValidationListener: this.hasPathValidationListener,
      hasDatagramListener: this.hasDatagramListener,
      hasDatagramStatusListener: this.hasDatagramStatusListener,
      hasSessionTicketListener: this.hasSessionTicketListener,
      hasNewTokenListener: this.hasNewTokenListener,
      hasOriginListener: this.hasOriginListener,
      isClosing: this.isClosing,
      isGracefulClose: this.isGracefulClose,
      isSilentClose: this.isSilentClose,
      isStatelessReset: this.isStatelessReset,
      isHandshakeCompleted: this.isHandshakeCompleted,
      isHandshakeConfirmed: this.isHandshakeConfirmed,
      isStreamOpenAllowed: this.isStreamOpenAllowed,
      isPrioritySupported: this.isPrioritySupported,
      headersSupported: this.headersSupported,
      isWrapped: this.isWrapped,
      applicationType: this.applicationType,
      noErrorCode: `${this.noErrorCode}`,
      internalErrorCode: `${this.internalErrorCode}`,
      maxDatagramSize: `${this.maxDatagramSize}`,
      lastDatagramId: `${this.lastDatagramId}`,
      maxPendingDatagrams: this.maxPendingDatagrams,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    if (DataViewPrototypeGetByteLength(this.#handle) === 0) {
      return 'QuicSessionState { <Closed> }';
    }

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `QuicSessionState ${inspect({
      hasPathValidationListener: this.hasPathValidationListener,
      hasDatagramListener: this.hasDatagramListener,
      hasDatagramStatusListener: this.hasDatagramStatusListener,
      hasSessionTicketListener: this.hasSessionTicketListener,
      hasNewTokenListener: this.hasNewTokenListener,
      hasOriginListener: this.hasOriginListener,
      isClosing: this.isClosing,
      isGracefulClose: this.isGracefulClose,
      isSilentClose: this.isSilentClose,
      isStatelessReset: this.isStatelessReset,
      isHandshakeCompleted: this.isHandshakeCompleted,
      isHandshakeConfirmed: this.isHandshakeConfirmed,
      isStreamOpenAllowed: this.isStreamOpenAllowed,
      isPrioritySupported: this.isPrioritySupported,
      headersSupported: this.headersSupported,
      isWrapped: this.isWrapped,
      applicationType: this.applicationType,
      noErrorCode: this.noErrorCode,
      internalErrorCode: this.internalErrorCode,
      maxDatagramSize: this.maxDatagramSize,
      lastDatagramId: this.lastDatagramId,
    }, opts)}`;
  }

  [kFinishClose]() {
    // Snapshot the state into a new DataView since the underlying
    // buffer will be destroyed.
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    this.#handle = new DataView(new ArrayBuffer(0));
  }
}

class QuicStreamState {
  /** @type {DataView} */
  #handle;

  /**
   * @param {symbol} privateSymbol
   * @param {ArrayBuffer} buffer
   */
  constructor(privateSymbol, buffer) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    if (!isArrayBuffer(buffer)) {
      throw new ERR_INVALID_ARG_TYPE('buffer', ['ArrayBuffer'], buffer);
    }
    this.#handle = new DataView(buffer);
  }

  /** @type {bigint} */
  get id() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetBigInt64(this.#handle, IDX_STATE_STREAM_ID, kIsLittleEndian);
  }

  /** @type {boolean} */
  get pending() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_PENDING);
  }

  /** @type {boolean} */
  get finSent() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_FIN_SENT);
  }

  /** @type {boolean} */
  get finReceived() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_FIN_RECEIVED);
  }

  /** @type {boolean} */
  get readEnded() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_READ_ENDED);
  }

  /** @type {boolean} */
  get writeEnded() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_WRITE_ENDED);
  }


  /** @type {boolean} */
  get reset() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_RESET);
  }

  /** @type {boolean} */
  get hasOutbound() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_HAS_OUTBOUND);
  }

  /** @type {boolean} */
  get hasReader() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_HAS_READER);
  }

  /** @type {boolean} */
  get wantsBlock() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_WANTS_BLOCK);
  }

  /** @type {boolean} */
  set wantsBlock(val) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_STREAM_WANTS_BLOCK, val ? 1 : 0);
  }

  /** @type {boolean} */
  get [kWantsHeaders]() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_WANTS_HEADERS);
  }

  /** @type {boolean} */
  set [kWantsHeaders](val) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_STREAM_WANTS_HEADERS, val ? 1 : 0);
  }

  /** @type {boolean} */
  get wantsReset() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_WANTS_RESET);
  }

  /** @type {boolean} */
  set wantsReset(val) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_STREAM_WANTS_RESET, val ? 1 : 0);
  }

  /** @type {boolean} */
  get [kWantsTrailers]() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_WANTS_TRAILERS);
  }

  /** @type {boolean} */
  set [kWantsTrailers](val) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_STREAM_WANTS_TRAILERS, val ? 1 : 0);
  }

  /** @type {boolean} */
  get early() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_RECEIVED_EARLY_DATA);
  }

  /** @type {bigint} */
  get resetCode() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetBigUint64(
      this.#handle, IDX_STATE_STREAM_RESET_CODE, kIsLittleEndian);
  }

  /** @type {bigint} */
  get writeDesiredSize() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetUint32(
      this.#handle, IDX_STATE_STREAM_WRITE_DESIRED_SIZE, kIsLittleEndian);
  }

  set writeDesiredSize(val) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    DataViewPrototypeSetUint32(
      this.#handle, IDX_STATE_STREAM_WRITE_DESIRED_SIZE, val, kIsLittleEndian);
  }

  /** @type {number} */
  get highWaterMark() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return undefined;
    return DataViewPrototypeGetUint32(
      this.#handle, IDX_STATE_STREAM_HIGH_WATER_MARK, kIsLittleEndian);
  }

  set highWaterMark(val) {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    DataViewPrototypeSetUint32(
      this.#handle, IDX_STATE_STREAM_HIGH_WATER_MARK, val, kIsLittleEndian);
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return {};
    return {
      __proto__: null,
      id: `${this.id}`,
      pending: this.pending,
      finSent: this.finSent,
      finReceived: this.finReceived,
      readEnded: this.readEnded,
      writeEnded: this.writeEnded,
      reset: this.reset,
      hasOutbound: this.hasOutbound,
      hasReader: this.hasReader,
      wantsBlock: this.wantsBlock,
      wantsReset: this.wantsReset,
      early: this.early,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    if (DataViewPrototypeGetByteLength(this.#handle) === 0) {
      return 'QuicStreamState { <Closed> }';
    }

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `QuicStreamState ${inspect({
      id: this.id,
      pending: this.pending,
      finSent: this.finSent,
      finReceived: this.finReceived,
      readEnded: this.readEnded,
      writeEnded: this.writeEnded,
      reset: this.reset,
      hasOutbound: this.hasOutbound,
      hasReader: this.hasReader,
      wantsBlock: this.wantsBlock,
      wantsReset: this.wantsReset,
      early: this.early,
    }, opts)}`;
  }

  [kFinishClose]() {
    // Snapshot the state into a new DataView since the underlying
    // buffer will be destroyed.
    if (DataViewPrototypeGetByteLength(this.#handle) === 0) return;
    this.#handle = new DataView(new ArrayBuffer(0));
  }
}

module.exports = {
  QuicEndpointState,
  QuicSessionState,
  QuicStreamState,
};

/* c8 ignore stop */

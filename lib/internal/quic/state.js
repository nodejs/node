'use strict';

const {
  ArrayBuffer,
  DataView,
  DataViewPrototypeGetBigInt64,
  DataViewPrototypeGetBigUint64,
  DataViewPrototypeGetUint8,
  DataViewPrototypeSetUint8,
  JSONStringify,
} = primordials;

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

const {
  isArrayBuffer,
} = require('util/types');

const { inspect } = require('internal/util/inspect');

const {
  kFinishClose,
  kInspect,
  kPrivateConstructor,
} = require('internal/quic/symbols');

// This file defines the helper objects for accessing state for
// various QUIC objects. Each of these wraps a DataView.
// Some of the state properties are read only, others are mutable.
// An ArrayBuffer is shared with the C++ level to allow for more
// efficient communication of state across the C++/JS boundary.
// When the state object is no longer needed, it is closed to
// prevent further updates to the buffer.

const {
  IDX_STATE_SESSION_PATH_VALIDATION,
  IDX_STATE_SESSION_VERSION_NEGOTIATION,
  IDX_STATE_SESSION_DATAGRAM,
  IDX_STATE_SESSION_SESSION_TICKET,
  IDX_STATE_SESSION_CLOSING,
  IDX_STATE_SESSION_GRACEFUL_CLOSE,
  IDX_STATE_SESSION_SILENT_CLOSE,
  IDX_STATE_SESSION_STATELESS_RESET,
  IDX_STATE_SESSION_DESTROYED,
  IDX_STATE_SESSION_HANDSHAKE_COMPLETED,
  IDX_STATE_SESSION_HANDSHAKE_CONFIRMED,
  IDX_STATE_SESSION_STREAM_OPEN_ALLOWED,
  IDX_STATE_SESSION_PRIORITY_SUPPORTED,
  IDX_STATE_SESSION_WRAPPED,
  IDX_STATE_SESSION_LAST_DATAGRAM_ID,

  IDX_STATE_ENDPOINT_BOUND,
  IDX_STATE_ENDPOINT_RECEIVING,
  IDX_STATE_ENDPOINT_LISTENING,
  IDX_STATE_ENDPOINT_CLOSING,
  IDX_STATE_ENDPOINT_BUSY,
  IDX_STATE_ENDPOINT_PENDING_CALLBACKS,

  IDX_STATE_STREAM_ID,
  IDX_STATE_STREAM_FIN_SENT,
  IDX_STATE_STREAM_FIN_RECEIVED,
  IDX_STATE_STREAM_READ_ENDED,
  IDX_STATE_STREAM_WRITE_ENDED,
  IDX_STATE_STREAM_DESTROYED,
  IDX_STATE_STREAM_PAUSED,
  IDX_STATE_STREAM_RESET,
  IDX_STATE_STREAM_HAS_READER,
  IDX_STATE_STREAM_WANTS_BLOCK,
  IDX_STATE_STREAM_WANTS_HEADERS,
  IDX_STATE_STREAM_WANTS_RESET,
  IDX_STATE_STREAM_WANTS_TRAILERS,
} = internalBinding('quic');

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

  #assertNotClosed() {
    if (this.#handle.byteLength === 0) {
      throw new ERR_INVALID_STATE('Endpoint is closed');
    }
  }

  /** @type {boolean} */
  get isBound() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_ENDPOINT_BOUND);
  }

  /** @type {boolean} */
  get isReceiving() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_ENDPOINT_RECEIVING);
  }

  /** @type {boolean} */
  get isListening() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_ENDPOINT_LISTENING);
  }

  /** @type {boolean} */
  get isClosing() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_ENDPOINT_CLOSING);
  }

  /** @type {boolean} */
  get isBusy() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_ENDPOINT_BUSY);
  }

  /**
   * The number of underlying callbacks that are pending. If the session
   * is closing, these are the number of callbacks that the session is
   * waiting on before it can be closed.
   * @type {bigint}
   */
  get pendingCallbacks() {
    this.#assertNotClosed();
    return DataViewPrototypeGetBigUint64(this.#handle, IDX_STATE_ENDPOINT_PENDING_CALLBACKS);
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    if (this.#handle.byteLength === 0) return {};
    return {
      __proto__: null,
      isBound: this.isBound,
      isReceiving: this.isReceiving,
      isListening: this.isListening,
      isClosing: this.isClosing,
      isBusy: this.isBusy,
      pendingCallbacks: `${this.pendingCallbacks}`,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    if (this.#handle.byteLength === 0) {
      return 'QuicEndpointState { <Closed> }';
    }

    const opts = {
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
    if (this.#handle.byteLength === 0) return;
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

  #assertNotClosed() {
    if (this.#handle.byteLength === 0) {
      throw new ERR_INVALID_STATE('Session is closed');
    }
  }

  /** @type {boolean} */
  get hasPathValidationListener() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_PATH_VALIDATION);
  }

  /** @type {boolean} */
  set hasPathValidationListener(val) {
    this.#assertNotClosed();
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_SESSION_PATH_VALIDATION, val ? 1 : 0);
  }

  /** @type {boolean} */
  get hasVersionNegotiationListener() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_VERSION_NEGOTIATION);
  }

  /** @type {boolean} */
  set hasVersionNegotiationListener(val) {
    this.#assertNotClosed();
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_SESSION_VERSION_NEGOTIATION, val ? 1 : 0);
  }

  /** @type {boolean} */
  get hasDatagramListener() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_DATAGRAM);
  }

  /** @type {boolean} */
  set hasDatagramListener(val) {
    this.#assertNotClosed();
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_SESSION_DATAGRAM, val ? 1 : 0);
  }

  /** @type {boolean} */
  get hasSessionTicketListener() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_SESSION_TICKET);
  }

  /** @type {boolean} */
  set hasSessionTicketListener(val) {
    this.#assertNotClosed();
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_SESSION_SESSION_TICKET, val ? 1 : 0);
  }

  /** @type {boolean} */
  get isClosing() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_CLOSING);
  }

  /** @type {boolean} */
  get isGracefulClose() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_GRACEFUL_CLOSE);
  }

  /** @type {boolean} */
  get isSilentClose() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_SILENT_CLOSE);
  }

  /** @type {boolean} */
  get isStatelessReset() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_STATELESS_RESET);
  }

  /** @type {boolean} */
  get isDestroyed() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_DESTROYED);
  }

  /** @type {boolean} */
  get isHandshakeCompleted() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_HANDSHAKE_COMPLETED);
  }

  /** @type {boolean} */
  get isHandshakeConfirmed() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_HANDSHAKE_CONFIRMED);
  }

  /** @type {boolean} */
  get isStreamOpenAllowed() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_STREAM_OPEN_ALLOWED);
  }

  /** @type {boolean} */
  get isPrioritySupported() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_PRIORITY_SUPPORTED);
  }

  /** @type {boolean} */
  get isWrapped() {
    this.#assertNotClosed();
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_SESSION_WRAPPED);
  }

  /** @type {bigint} */
  get lastDatagramId() {
    this.#assertNotClosed();
    return DataViewPrototypeGetBigUint64(this.#handle, IDX_STATE_SESSION_LAST_DATAGRAM_ID);
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    if (this.#handle.byteLength === 0) return {};
    return {
      __proto__: null,
      hasPathValidationListener: this.hasPathValidationListener,
      hasVersionNegotiationListener: this.hasVersionNegotiationListener,
      hasDatagramListener: this.hasDatagramListener,
      hasSessionTicketListener: this.hasSessionTicketListener,
      isClosing: this.isClosing,
      isGracefulClose: this.isGracefulClose,
      isSilentClose: this.isSilentClose,
      isStatelessReset: this.isStatelessReset,
      isDestroyed: this.isDestroyed,
      isHandshakeCompleted: this.isHandshakeCompleted,
      isHandshakeConfirmed: this.isHandshakeConfirmed,
      isStreamOpenAllowed: this.isStreamOpenAllowed,
      isPrioritySupported: this.isPrioritySupported,
      isWrapped: this.isWrapped,
      lastDatagramId: `${this.lastDatagramId}`,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    if (this.#handle.byteLength === 0) {
      return 'QuicSessionState { <Closed> }';
    }

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `QuicSessionState ${inspect({
      hasPathValidationListener: this.hasPathValidationListener,
      hasVersionNegotiationListener: this.hasVersionNegotiationListener,
      hasDatagramListener: this.hasDatagramListener,
      hasSessionTicketListener: this.hasSessionTicketListener,
      isClosing: this.isClosing,
      isGracefulClose: this.isGracefulClose,
      isSilentClose: this.isSilentClose,
      isStatelessReset: this.isStatelessReset,
      isDestroyed: this.isDestroyed,
      isHandshakeCompleted: this.isHandshakeCompleted,
      isHandshakeConfirmed: this.isHandshakeConfirmed,
      isStreamOpenAllowed: this.isStreamOpenAllowed,
      isPrioritySupported: this.isPrioritySupported,
      isWrapped: this.isWrapped,
      lastDatagramId: this.lastDatagramId,
    }, opts)}`;
  }

  [kFinishClose]() {
    // Snapshot the state into a new DataView since the underlying
    // buffer will be destroyed.
    if (this.#handle.byteLength === 0) return;
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
    return DataViewPrototypeGetBigInt64(this.#handle, IDX_STATE_STREAM_ID);
  }

  /** @type {boolean} */
  get finSent() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_FIN_SENT);
  }

  /** @type {boolean} */
  get finReceived() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_FIN_RECEIVED);
  }

  /** @type {boolean} */
  get readEnded() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_READ_ENDED);
  }

  /** @type {boolean} */
  get writeEnded() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_WRITE_ENDED);
  }

  /** @type {boolean} */
  get destroyed() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_DESTROYED);
  }

  /** @type {boolean} */
  get paused() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_PAUSED);
  }

  /** @type {boolean} */
  get reset() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_RESET);
  }

  /** @type {boolean} */
  get hasReader() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_HAS_READER);
  }

  /** @type {boolean} */
  get wantsBlock() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_WANTS_BLOCK);
  }

  /** @type {boolean} */
  set wantsBlock(val) {
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_STREAM_WANTS_BLOCK, val ? 1 : 0);
  }

  /** @type {boolean} */
  get wantsHeaders() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_WANTS_HEADERS);
  }

  /** @type {boolean} */
  set wantsHeaders(val) {
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_STREAM_WANTS_HEADERS, val ? 1 : 0);
  }

  /** @type {boolean} */
  get wantsReset() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_WANTS_RESET);
  }

  /** @type {boolean} */
  set wantsReset(val) {
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_STREAM_WANTS_RESET, val ? 1 : 0);
  }

  /** @type {boolean} */
  get wantsTrailers() {
    return !!DataViewPrototypeGetUint8(this.#handle, IDX_STATE_STREAM_WANTS_TRAILERS);
  }

  /** @type {boolean} */
  set wantsTrailers(val) {
    DataViewPrototypeSetUint8(this.#handle, IDX_STATE_STREAM_WANTS_TRAILERS, val ? 1 : 0);
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    if (this.#handle.byteLength === 0) return {};
    return {
      __proto__: null,
      id: `${this.id}`,
      finSent: this.finSent,
      finReceived: this.finReceived,
      readEnded: this.readEnded,
      writeEnded: this.writeEnded,
      destroyed: this.destroyed,
      paused: this.paused,
      reset: this.reset,
      hasReader: this.hasReader,
      wantsBlock: this.wantsBlock,
      wantsHeaders: this.wantsHeaders,
      wantsReset: this.wantsReset,
      wantsTrailers: this.wantsTrailers,
    };
  }

  [kInspect](depth, options) {
    if (depth < 0)
      return this;

    if (this.#handle.byteLength === 0) {
      return 'QuicStreamState { <Closed> }';
    }

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `QuicStreamState ${inspect({
      id: this.id,
      finSent: this.finSent,
      finReceived: this.finReceived,
      readEnded: this.readEnded,
      writeEnded: this.writeEnded,
      destroyed: this.destroyed,
      paused: this.paused,
      reset: this.reset,
      hasReader: this.hasReader,
      wantsBlock: this.wantsBlock,
      wantsHeaders: this.wantsHeaders,
      wantsReset: this.wantsReset,
      wantsTrailers: this.wantsTrailers,
    }, opts)}`;
  }

  [kFinishClose]() {
    // Snapshot the state into a new DataView since the underlying
    // buffer will be destroyed.
    if (this.#handle.byteLength === 0) return;
    this.#handle = new DataView(new ArrayBuffer(0));
  }
}

module.exports = {
  QuicEndpointState,
  QuicSessionState,
  QuicStreamState,
};

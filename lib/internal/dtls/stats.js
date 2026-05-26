'use strict';

// TODO(@jasnell) Temporarily ignoring c8 coverage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  BigUint64Array,
  JSONStringify,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

if (!process.features.dtls || !getOptionValue('--experimental-dtls')) {
  return;
}

const {
  isArrayBuffer,
} = require('util/types');

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');

const { inspect } = require('internal/util/inspect');
const assert = require('internal/assert');

const {
  kFinishClose,
  kPrivateConstructor,
} = require('internal/dtls/symbols');

// This file defines the helper objects for accessing statistics collected
// by DTLS endpoints and sessions. Each wraps a BigUint64Array backed by
// a shared ArrayBuffer that is updated by the C++ internals.

const {
  IDX_STATS_ENDPOINT_CREATED_AT,
  IDX_STATS_ENDPOINT_DESTROYED_AT,
  IDX_STATS_ENDPOINT_BYTES_RECEIVED,
  IDX_STATS_ENDPOINT_BYTES_SENT,
  IDX_STATS_ENDPOINT_PACKETS_RECEIVED,
  IDX_STATS_ENDPOINT_PACKETS_SENT,
  IDX_STATS_ENDPOINT_SERVER_SESSIONS,
  IDX_STATS_ENDPOINT_CLIENT_SESSIONS,
  IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT,

  IDX_STATS_SESSION_CREATED_AT,
  IDX_STATS_SESSION_DESTROYED_AT,
  IDX_STATS_SESSION_CLOSING_AT,
  IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT,
  IDX_STATS_SESSION_BYTES_RECEIVED,
  IDX_STATS_SESSION_BYTES_SENT,
  IDX_STATS_SESSION_MESSAGES_RECEIVED,
  IDX_STATS_SESSION_MESSAGES_SENT,
  IDX_STATS_SESSION_RETRANSMIT_COUNT,
} = internalBinding('dtls');

assert(IDX_STATS_ENDPOINT_CREATED_AT !== undefined);
assert(IDX_STATS_ENDPOINT_DESTROYED_AT !== undefined);
assert(IDX_STATS_ENDPOINT_BYTES_RECEIVED !== undefined);
assert(IDX_STATS_ENDPOINT_BYTES_SENT !== undefined);
assert(IDX_STATS_ENDPOINT_PACKETS_RECEIVED !== undefined);
assert(IDX_STATS_ENDPOINT_PACKETS_SENT !== undefined);
assert(IDX_STATS_ENDPOINT_SERVER_SESSIONS !== undefined);
assert(IDX_STATS_ENDPOINT_CLIENT_SESSIONS !== undefined);
assert(IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT !== undefined);
assert(IDX_STATS_SESSION_CREATED_AT !== undefined);
assert(IDX_STATS_SESSION_DESTROYED_AT !== undefined);
assert(IDX_STATS_SESSION_CLOSING_AT !== undefined);
assert(IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT !== undefined);
assert(IDX_STATS_SESSION_BYTES_RECEIVED !== undefined);
assert(IDX_STATS_SESSION_BYTES_SENT !== undefined);
assert(IDX_STATS_SESSION_MESSAGES_RECEIVED !== undefined);
assert(IDX_STATS_SESSION_MESSAGES_SENT !== undefined);
assert(IDX_STATS_SESSION_RETRANSMIT_COUNT !== undefined);

class DTLSEndpointStats {
  /** @type {BigUint64Array} */
  #handle;
  /** @type {boolean} */
  #disconnected = false;

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
    this.#handle = new BigUint64Array(buffer);
  }

  /** @type {bigint} */
  get createdAt() {
    return this.#handle[IDX_STATS_ENDPOINT_CREATED_AT];
  }

  /** @type {bigint} */
  get destroyedAt() {
    return this.#handle[IDX_STATS_ENDPOINT_DESTROYED_AT];
  }

  /** @type {bigint} */
  get bytesReceived() {
    return this.#handle[IDX_STATS_ENDPOINT_BYTES_RECEIVED];
  }

  /** @type {bigint} */
  get bytesSent() {
    return this.#handle[IDX_STATS_ENDPOINT_BYTES_SENT];
  }

  /** @type {bigint} */
  get packetsReceived() {
    return this.#handle[IDX_STATS_ENDPOINT_PACKETS_RECEIVED];
  }

  /** @type {bigint} */
  get packetsSent() {
    return this.#handle[IDX_STATS_ENDPOINT_PACKETS_SENT];
  }

  /** @type {bigint} */
  get serverSessions() {
    return this.#handle[IDX_STATS_ENDPOINT_SERVER_SESSIONS];
  }

  /** @type {bigint} */
  get clientSessions() {
    return this.#handle[IDX_STATS_ENDPOINT_CLIENT_SESSIONS];
  }

  /** @type {bigint} */
  get serverBusyCount() {
    return this.#handle[IDX_STATS_ENDPOINT_SERVER_BUSY_COUNT];
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    return {
      __proto__: null,
      connected: this.isConnected,
      createdAt: `${this.createdAt}`,
      destroyedAt: `${this.destroyedAt}`,
      bytesReceived: `${this.bytesReceived}`,
      bytesSent: `${this.bytesSent}`,
      packetsReceived: `${this.packetsReceived}`,
      packetsSent: `${this.packetsSent}`,
      serverSessions: `${this.serverSessions}`,
      clientSessions: `${this.clientSessions}`,
      serverBusyCount: `${this.serverBusyCount}`,
    };
  }

  [inspect.custom](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `DTLSEndpointStats ${inspect({
      connected: this.isConnected,
      createdAt: this.createdAt,
      destroyedAt: this.destroyedAt,
      bytesReceived: this.bytesReceived,
      bytesSent: this.bytesSent,
      packetsReceived: this.packetsReceived,
      packetsSent: this.packetsSent,
      serverSessions: this.serverSessions,
      clientSessions: this.clientSessions,
      serverBusyCount: this.serverBusyCount,
    }, opts)}`;
  }

  /**
   * True if this stats object is still connected to the underlying
   * stats source. If false, the stats are stale.
   * @type {boolean}
   */
  get isConnected() {
    return !this.#disconnected;
  }

  [kFinishClose]() {
    // Snapshot the stats into a new BigUint64Array since the underlying
    // buffer will be destroyed.
    this.#handle = new BigUint64Array(this.#handle);
    this.#disconnected = true;
  }
}

class DTLSSessionStats {
  /** @type {BigUint64Array} */
  #handle;
  /** @type {boolean} */
  #disconnected = false;

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
    this.#handle = new BigUint64Array(buffer);
  }

  /** @type {bigint} */
  get createdAt() {
    return this.#handle[IDX_STATS_SESSION_CREATED_AT];
  }

  /** @type {bigint} */
  get destroyedAt() {
    return this.#handle[IDX_STATS_SESSION_DESTROYED_AT];
  }

  /** @type {bigint} */
  get closingAt() {
    return this.#handle[IDX_STATS_SESSION_CLOSING_AT];
  }

  /** @type {bigint} */
  get handshakeCompletedAt() {
    return this.#handle[IDX_STATS_SESSION_HANDSHAKE_COMPLETED_AT];
  }

  /** @type {bigint} */
  get bytesReceived() {
    return this.#handle[IDX_STATS_SESSION_BYTES_RECEIVED];
  }

  /** @type {bigint} */
  get bytesSent() {
    return this.#handle[IDX_STATS_SESSION_BYTES_SENT];
  }

  /** @type {bigint} */
  get messagesReceived() {
    return this.#handle[IDX_STATS_SESSION_MESSAGES_RECEIVED];
  }

  /** @type {bigint} */
  get messagesSent() {
    return this.#handle[IDX_STATS_SESSION_MESSAGES_SENT];
  }

  /** @type {bigint} */
  get retransmitCount() {
    return this.#handle[IDX_STATS_SESSION_RETRANSMIT_COUNT];
  }

  toString() {
    return JSONStringify(this.toJSON());
  }

  toJSON() {
    return {
      __proto__: null,
      connected: this.isConnected,
      createdAt: `${this.createdAt}`,
      destroyedAt: `${this.destroyedAt}`,
      closingAt: `${this.closingAt}`,
      handshakeCompletedAt: `${this.handshakeCompletedAt}`,
      bytesReceived: `${this.bytesReceived}`,
      bytesSent: `${this.bytesSent}`,
      messagesReceived: `${this.messagesReceived}`,
      messagesSent: `${this.messagesSent}`,
      retransmitCount: `${this.retransmitCount}`,
    };
  }

  [inspect.custom](depth, options) {
    if (depth < 0)
      return this;

    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `DTLSSessionStats ${inspect({
      connected: this.isConnected,
      createdAt: this.createdAt,
      destroyedAt: this.destroyedAt,
      closingAt: this.closingAt,
      handshakeCompletedAt: this.handshakeCompletedAt,
      bytesReceived: this.bytesReceived,
      bytesSent: this.bytesSent,
      messagesReceived: this.messagesReceived,
      messagesSent: this.messagesSent,
      retransmitCount: this.retransmitCount,
    }, opts)}`;
  }

  /**
   * True if this stats object is still connected to the underlying
   * stats source. If false, the stats are stale.
   * @type {boolean}
   */
  get isConnected() {
    return !this.#disconnected;
  }

  [kFinishClose]() {
    // Snapshot the stats into a new BigUint64Array since the underlying
    // buffer will be destroyed.
    this.#handle = new BigUint64Array(this.#handle);
    this.#disconnected = true;
  }
}

module.exports = {
  DTLSEndpointStats,
  DTLSSessionStats,
};

/* c8 ignore stop */

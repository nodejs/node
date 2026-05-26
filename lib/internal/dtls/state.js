'use strict';

// TODO(@jasnell) Temporarily ignoring c8 coverage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  DataView,
  DataViewPrototypeGetByteLength,
  DataViewPrototypeGetUint32,
  DataViewPrototypeGetUint8,
  DataViewPrototypeSetUint8,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

if (!process.features.dtls || !getOptionValue('--experimental-dtls')) {
  return;
}

const {
  codes: {
    ERR_ILLEGAL_CONSTRUCTOR,
    ERR_INVALID_STATE,
  },
} = require('internal/errors');

const {
  kPrivateConstructor,
} = require('internal/dtls/symbols');

const {
  IDX_ENDPOINT_STATE_BOUND,
  IDX_ENDPOINT_STATE_LISTENING,
  IDX_ENDPOINT_STATE_CLOSING,
  IDX_ENDPOINT_STATE_DESTROYED,
  IDX_ENDPOINT_STATE_SESSION_COUNT,
  IDX_ENDPOINT_STATE_BUSY,
  IDX_SESSION_STATE_HANDSHAKING,
  IDX_SESSION_STATE_OPEN,
  IDX_SESSION_STATE_CLOSING,
  IDX_SESSION_STATE_DESTROYED,
  IDX_SESSION_STATE_HAS_MESSAGE_LISTENER,
} = internalBinding('dtls');

function isAlive(view) {
  return DataViewPrototypeGetByteLength(view) > 0;
}

// DTLSEndpointState wraps the shared ArrayBuffer from C++.
// The C++ struct layout (DTLSEndpointStateData) is:
//   uint8_t bound;          // offset 0
//   uint8_t listening;      // offset 1
//   uint8_t closing;        // offset 2
//   uint8_t destroyed;      // offset 3
//   uint32_t session_count; // offset 4 (4-byte aligned)
//   uint8_t busy;           // offset 8
class DTLSEndpointState {
  #handle;

  constructor(privateSymbol, buffer) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    this.#handle = new DataView(buffer);
  }

  get bound() {
    if (!isAlive(this.#handle)) return false;
    return DataViewPrototypeGetUint8(
      this.#handle, IDX_ENDPOINT_STATE_BOUND) === 1;
  }

  get listening() {
    if (!isAlive(this.#handle)) return false;
    return DataViewPrototypeGetUint8(
      this.#handle, IDX_ENDPOINT_STATE_LISTENING) === 1;
  }

  get closing() {
    if (!isAlive(this.#handle)) return false;
    return DataViewPrototypeGetUint8(
      this.#handle, IDX_ENDPOINT_STATE_CLOSING) === 1;
  }

  get destroyed() {
    if (!isAlive(this.#handle)) return true;
    return DataViewPrototypeGetUint8(
      this.#handle, IDX_ENDPOINT_STATE_DESTROYED) === 1;
  }

  get sessionCount() {
    if (!isAlive(this.#handle)) return 0;
    return DataViewPrototypeGetUint32(
      this.#handle, IDX_ENDPOINT_STATE_SESSION_COUNT, true);
  }

  get busy() {
    if (!isAlive(this.#handle)) return false;
    return DataViewPrototypeGetUint8(
      this.#handle, IDX_ENDPOINT_STATE_BUSY) === 1;
  }

  set busy(val) {
    if (!isAlive(this.#handle)) {
      throw new ERR_INVALID_STATE('Endpoint is destroyed');
    }
    DataViewPrototypeSetUint8(
      this.#handle, IDX_ENDPOINT_STATE_BUSY, val ? 1 : 0);
  }
}

// DTLSSessionState wraps the shared ArrayBuffer from C++.
// The C++ struct layout (DTLSSessionStateData) is:
//   uint8_t handshaking;          // offset 0
//   uint8_t open;                 // offset 1
//   uint8_t closing;              // offset 2
//   uint8_t destroyed;            // offset 3
//   uint8_t has_message_listener; // offset 4
class DTLSSessionState {
  #handle;

  constructor(privateSymbol, buffer) {
    if (privateSymbol !== kPrivateConstructor) {
      throw new ERR_ILLEGAL_CONSTRUCTOR();
    }
    this.#handle = new DataView(buffer);
  }

  get handshaking() {
    if (!isAlive(this.#handle)) return false;
    return DataViewPrototypeGetUint8(
      this.#handle, IDX_SESSION_STATE_HANDSHAKING) === 1;
  }

  get open() {
    if (!isAlive(this.#handle)) return false;
    return DataViewPrototypeGetUint8(
      this.#handle, IDX_SESSION_STATE_OPEN) === 1;
  }

  get closing() {
    if (!isAlive(this.#handle)) return false;
    return DataViewPrototypeGetUint8(
      this.#handle, IDX_SESSION_STATE_CLOSING) === 1;
  }

  get destroyed() {
    if (!isAlive(this.#handle)) return true;
    return DataViewPrototypeGetUint8(
      this.#handle, IDX_SESSION_STATE_DESTROYED) === 1;
  }

  get hasMessageListener() {
    if (!isAlive(this.#handle)) return false;
    return DataViewPrototypeGetUint8(
      this.#handle, IDX_SESSION_STATE_HAS_MESSAGE_LISTENER) === 1;
  }

  set hasMessageListener(val) {
    if (!isAlive(this.#handle)) return;
    DataViewPrototypeSetUint8(
      this.#handle, IDX_SESSION_STATE_HAS_MESSAGE_LISTENER, val ? 1 : 0);
  }
}

module.exports = {
  DTLSEndpointState,
  DTLSSessionState,
};

/* c8 ignore stop */

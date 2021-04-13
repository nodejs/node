'use strict';

const {
  PromisePrototypeThen,
  PromiseResolve,
} = primordials;

const {
  initializeCallbacks,
} = internalBinding('quic');

// If the initializeCallbacks is undefined, the Node.js binary
// was built without QUIC support, in which case we
// don't want to export anything here.
if (initializeCallbacks === undefined)
  return;

const { owner_symbol } = internalBinding('symbols');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_STATE,
    ERR_QUIC_VERSION_NEGOTIATION,
  },
} = require('internal/errors');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');

const {
  kAddSession,
  kBlocked,
  kClientHello,
  kClose,
  kCreatedStream,
  kDestroy,
  kHandshakeComplete,
  kHeaders,
  kOCSP,
  kResetStream,
  kSessionTicket,
  kTrailers,
  createDatagramEvent,
} = require('internal/quic/common');

const {
  TextEncoder,
} = require('util');

const assert = require('internal/assert');

function onEndpointError(error) {
  assert(error !== undefined);
  this[owner_symbol].destroy(error);
}

function onEndpointDone() {
  this[owner_symbol].destroy();
}

function onSessionNew(sessionHandle) {
  this[owner_symbol][kAddSession](sessionHandle);
}

function onSessionClientHello(alpn, servername, ciphers) {
  let alreadyDone = false;
  this[owner_symbol][kClientHello](alpn, servername, ciphers, (context) => {
    if (context !== undefined &&
        typeof context?.context?.external !== 'object') {
      throw new ERR_INVALID_ARG_TYPE(
        'context',
        'crypto.SecureContext',
        context);
    }
    if (alreadyDone)
      throw new ERR_INVALID_STATE('Client hello already completed');
    this.onClientHelloDone(context);
    alreadyDone = true;
  });
}

function onSessionClose(errorCode, errorType, silent, statelessReset) {
  this[owner_symbol][kClose](errorCode, errorType, silent, statelessReset);
}

function onSessionDatagram(buffer, early) {
  const session = this[owner_symbol];
  PromisePrototypeThen(
    PromiseResolve(),
    () => {
      try {
        session.dispatchEvent(createDatagramEvent(buffer, early, session));
      } catch (error) {
        // The catch here is purely defensive. If the event handler throws,
        // the error will be throw in a process.nextTick, making it impossible
        // for us to catch and handle here. This catch is a protection against
        // possible bugs in the Node.js event target implementation. As such,
        // this should never actually fire and coverage should report this as
        // not being tested.
        session[kDestroy](error);
      }
    }, () => {});  // Non-op error handling here because this will never reject.
}

function onSessionHandshake(
  servername,
  alpn,
  ciphername,
  cipherversion,
  maxPacketLength,
  validationErrorReason,
  validationErrorCode,
  earlyData) {
  this[owner_symbol][kHandshakeComplete](
    servername,
    alpn,
    ciphername,
    cipherversion,
    maxPacketLength,
    validationErrorReason,
    validationErrorCode,
    earlyData);
}

function onSessionOcspRequest(certificate, issuer) {
  let alreadyResponded = false;

  this[owner_symbol][kOCSP](
    'request', {
      certificate,
      issuer,
      callback: (response) => {
        if (alreadyResponded)
          throw new ERR_INVALID_STATE('OCSP response is already provided');
        if (response !== undefined) {
          if (typeof response === 'string') {
            const enc = new TextEncoder();
            response = enc.encode(response);
          } else if (!isArrayBufferView(response) &&
                     !isAnyArrayBuffer(response)) {
            throw new ERR_INVALID_ARG_TYPE(
              'response',
              [
                'ArrayBuffer',
                'SharedArrayBuffer',
                'TypedArray',
                'DataView',
                'Buffer',
              ],
              response);
          }
        }
        this.onOCSPDone(response);
        alreadyResponded = true;
      }
    });
}

function onSessionOcspResponse(response) {
  this[owner_symbol][kOCSP]('response', { response });
}

function onSessionTicket(ticket, transportParams) {
  this[owner_symbol][kSessionTicket](ticket, transportParams);
}

function onSessionVersionNegotiation(protocol, requested, supported) {
  const err = new ERR_QUIC_VERSION_NEGOTIATION();
  err.detail = {
    protocol,
    requested,
    supported,
  };
  this[owner_symbol].destroy(err);
}

function onStreamClose(error) {
  this[owner_symbol][kDestroy](error);
}

function onStreamReset(error) {
  this[owner_symbol][kResetStream](error);
}

function onStreamCreated(handle) {
  this[owner_symbol][kCreatedStream](handle);
}

function onStreamHeaders(headers, kind) {
  this[owner_symbol][kHeaders](headers, kind);
}

function onStreamBlocked() {
  this[owner_symbol][kBlocked]();
}

function onStreamTrailers() {
  this[owner_symbol][kTrailers]();
}

let initialized = false;

module.exports = {
  initializeBinding() {
    if (initialized) return;
    initialized = true;
    initializeCallbacks({
      onEndpointDone,
      onEndpointError,
      onSessionNew,
      onSessionClientHello,
      onSessionClose,
      onSessionDatagram,
      onSessionHandshake,
      onSessionOcspRequest,
      onSessionOcspResponse,
      onSessionTicket,
      onSessionVersionNegotiation,
      onStreamClose,
      onStreamCreated,
      onStreamReset,
      onStreamHeaders,
      onStreamBlocked,
      onStreamTrailers,
    });
  }
};

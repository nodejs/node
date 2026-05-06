'use strict';

// TODO(@jasnell) Temporarily ignoring c8 covrerage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  Symbol,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

if (!process.features.quic || !getOptionValue('--experimental-quic')) {
  return;
}

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const {
  kHandle: kKeyObjectHandle,
} = require('internal/crypto/util');

// Symbols used to hide various private properties and methods from the
// public API.

const kAttachFileHandle = Symbol('kAttachFileHandle');
const kBlocked = Symbol('kBlocked');
const kConnect = Symbol('kConnect');
const kDrain = Symbol('kDrain');
const kDatagram = Symbol('kDatagram');
const kDatagramStatus = Symbol('kDatagramStatus');
const kEarlyDataRejected = Symbol('kEarlyDataRejected');
const kFinishClose = Symbol('kFinishClose');
const kGoaway = Symbol('kGoaway');
const kHandshake = Symbol('kHandshake');
const kHandshakeCompleted = Symbol('kHandshakeCompleted');
const kHeaders = Symbol('kHeaders');
const kKeylog = Symbol('kKeylog');
const kListen = Symbol('kListen');
const kQlog = Symbol('kQlog');
const kNewSession = Symbol('kNewSession');
const kNewStream = Symbol('kNewStream');
const kNewToken = Symbol('kNewToken');
const kStreamCallbacks = Symbol('kStreamCallbacks');
const kOrigin = Symbol('kOrigin');
const kOwner = Symbol('kOwner');
const kPathValidation = Symbol('kPathValidation');
const kPrivateConstructor = Symbol('kPrivateConstructor');
const kRemoveSession = Symbol('kRemoveSession');
const kRemoveStream = Symbol('kRemoveStream');
const kReset = Symbol('kReset');
const kSendHeaders = Symbol('kSendHeaders');
const kSessionTicket = Symbol('kSessionTicket');
const kTrailers = Symbol('kTrailers');
const kVersionNegotiation = Symbol('kVersionNegotiation');
const kWantsHeaders = Symbol('kWantsHeaders');
const kWantsTrailers = Symbol('kWantsTrailers');

module.exports = {
  kAttachFileHandle,
  kBlocked,
  kConnect,
  kDatagram,
  kDatagramStatus,
  kDrain,
  kEarlyDataRejected,
  kFinishClose,
  kGoaway,
  kHandshake,
  kHandshakeCompleted,
  kHeaders,
  kInspect,
  kKeylog,
  kKeyObjectHandle,
  kListen,
  kNewSession,
  kNewStream,
  kNewToken,
  kStreamCallbacks,
  kOrigin,
  kOwner,
  kQlog,
  kPathValidation,
  kPrivateConstructor,
  kRemoveSession,
  kRemoveStream,
  kReset,
  kSendHeaders,
  kSessionTicket,
  kTrailers,
  kVersionNegotiation,
  kWantsHeaders,
  kWantsTrailers,
};

/* c8 ignore stop */

'use strict';

const {
  Symbol,
} = primordials;

const {
  customInspectSymbol: kInspect,
} = require('internal/util');

const {
  kHandle: kKeyObjectHandle,
  kKeyObject: kKeyObjectInner,
} = require('internal/crypto/util');

// Symbols used to hide various private properties and methods from the
// public API.

const kBlocked = Symbol('kBlocked');
const kDatagram = Symbol('kDatagram');
const kDatagramStatus = Symbol('kDatagramStatus');
const kError = Symbol('kError');
const kFinishClose = Symbol('kFinishClose');
const kHandshake = Symbol('kHandshake');
const kHeaders = Symbol('kHeaders');
const kOwner = Symbol('kOwner');
const kRemoveSession = Symbol('kRemoveSession');
const kNewSession = Symbol('kNewSession');
const kRemoveStream = Symbol('kRemoveStream');
const kNewStream = Symbol('kNewStream');
const kPathValidation = Symbol('kPathValidation');
const kReset = Symbol('kReset');
const kSessionTicket = Symbol('kSessionTicket');
const kTrailers = Symbol('kTrailers');
const kVersionNegotiation = Symbol('kVersionNegotiation');
const kPrivateConstructor = Symbol('kPrivateConstructor');

module.exports = {
  kBlocked,
  kDatagram,
  kDatagramStatus,
  kError,
  kFinishClose,
  kHandshake,
  kHeaders,
  kOwner,
  kRemoveSession,
  kNewSession,
  kRemoveStream,
  kNewStream,
  kPathValidation,
  kReset,
  kSessionTicket,
  kTrailers,
  kVersionNegotiation,
  kInspect,
  kKeyObjectHandle,
  kKeyObjectInner,
  kPrivateConstructor,
};

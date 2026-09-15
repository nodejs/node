'use strict';

// TODO(@jasnell) Temporarily ignoring c8 coverage for this file while tests
// are still being developed.
/* c8 ignore start */

const {
  Symbol,
} = primordials;

const {
  getOptionValue,
} = require('internal/options');

if (!process.features.dtls || !getOptionValue('--experimental-dtls')) {
  return;
}

module.exports = {
  // Private symbols for internal communication between classes.
  kOwner: Symbol('kOwner'),
  kHandle: Symbol('kHandle'),
  kListen: Symbol('kListen'),
  kConnect: Symbol('kConnect'),
  kFinishClose: Symbol('kFinishClose'),
  kNewSession: Symbol('kNewSession'),
  kRemoveSession: Symbol('kRemoveSession'),
  kHandshake: Symbol('kHandshake'),
  kReceive: Symbol('kReceive'),
  kError: Symbol('kError'),
  kClose: Symbol('kClose'),
  kMessage: Symbol('kMessage'),
  kKeylog: Symbol('kKeylog'),
  kTicket: Symbol('kTicket'),
  kPrivateConstructor: Symbol('kPrivateConstructor'),
  kSessionHandshake: Symbol('dtls.session.handshake'),
  kSessionMessage: Symbol('dtls.session.message'),
  kSessionError: Symbol('dtls.session.error'),
  kSessionClose: Symbol('dtls.session.close'),
  kSessionKeylog: Symbol('dtls.session.keylog'),
};

/* c8 ignore stop */

'use strict';

const {
  ObjectCreate,
  ObjectSeal,
} = primordials;

const {
  emitExperimentalWarning,
} = require('internal/util');
emitExperimentalWarning('dtls');

const {
  connect,
  listen,
  DTLSEndpoint,
  DTLSSession,
} = require('internal/dtls/dtls');

function getEnumerableConstant(value) {
  return {
    __proto__: null,
    value,
    enumerable: true,
    configurable: false,
    writable: false,
  };
}

module.exports = ObjectSeal(ObjectCreate(null, {
  __proto__: null,
  connect: getEnumerableConstant(connect),
  listen: getEnumerableConstant(listen),
  DTLSEndpoint: getEnumerableConstant(DTLSEndpoint),
  DTLSSession: getEnumerableConstant(DTLSSession),
}));

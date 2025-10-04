'use strict';

const {
  ObjectCreate,
  ObjectSeal,
} = primordials;

const {
  emitExperimentalWarning,
} = require('internal/util');
emitExperimentalWarning('quic');

const {
  connect,
  listen,
  QuicEndpoint,
  QuicSession,
  QuicStream,
  CC_ALGO_RENO,
  CC_ALGO_CUBIC,
  CC_ALGO_BBR,
  DEFAULT_CIPHERS,
  DEFAULT_GROUPS,
} = require('internal/quic/quic');

function getEnumerableConstant(value) {
  return {
    __proto__: null,
    value,
    enumerable: true,
    configurable: false,
    writable: false,
  };
}

const cc = ObjectSeal(ObjectCreate(null, {
  __proto__: null,
  RENO: getEnumerableConstant(CC_ALGO_RENO),
  CUBIC: getEnumerableConstant(CC_ALGO_CUBIC),
  BBR: getEnumerableConstant(CC_ALGO_BBR),
}));

const constants = ObjectSeal(ObjectCreate(null, {
  __proto__: null,
  cc: getEnumerableConstant(cc),
  DEFAULT_CIPHERS: getEnumerableConstant(DEFAULT_CIPHERS),
  DEFAULT_GROUPS: getEnumerableConstant(DEFAULT_GROUPS),
}));

module.exports = ObjectSeal(ObjectCreate(null, {
  __proto__: null,
  connect: getEnumerableConstant(connect),
  listen: getEnumerableConstant(listen),
  QuicEndpoint: getEnumerableConstant(QuicEndpoint),
  QuicSession: getEnumerableConstant(QuicSession),
  QuicStream: getEnumerableConstant(QuicStream),
  constants: getEnumerableConstant(constants),
}));

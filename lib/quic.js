'use strict';

const {
  emitExperimentalWarning,
} = require('internal/util');
emitExperimentalWarning('quic');

const {
  connect,
  listen,
  listEndpoints,
  QuicEndpoint,
  QuicError,
  QuicSession,
  QuicStream,
  CC_ALGO_RENO,
  CC_ALGO_CUBIC,
  CC_ALGO_BBR,
  DEFAULT_CIPHERS,
  DEFAULT_GROUPS,
} = require('internal/quic/quic');

const cc = {
  get RENO() { return CC_ALGO_RENO; },
  get CUBIC() { return CC_ALGO_CUBIC; },
  get BBR() { return CC_ALGO_BBR; },
};

const constants = {
  get cc() { return cc; },
  get DEFAULT_CIPHERS() { return DEFAULT_CIPHERS; },
  get DEFAULT_GROUPS() { return DEFAULT_GROUPS; },
};

module.exports = {
  connect,
  listen,
  listEndpoints,
  QuicEndpoint,
  QuicError,
  QuicSession,
  QuicStream,
  get constants() {
    return constants;
  },
};

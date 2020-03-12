'use strict';

const {
  codes: {
    ERR_QUIC_UNAVAILABLE,
  },
} = require('internal/errors');
const { assertCrypto } = require('internal/util');

assertCrypto();
if (process.versions.ngtcp2 === undefined)
  throw new ERR_QUIC_UNAVAILABLE();

const {
  createSocket
} = require('internal/quic/core');

module.exports = { createSocket };

process.emitWarning(
  'QUIC protocol support is experimental and not yet ' +
  'supported for production use',
  'ExperimentalWarning');

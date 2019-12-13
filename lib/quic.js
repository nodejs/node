'use strict';

const {
  createSocket
} = require('internal/quic/core');

module.exports = { createSocket };

process.emitWarning(
  'QUIC protocol support is experimental and not yet ' +
  'supported for production use',
  'ExperimentalWarning');

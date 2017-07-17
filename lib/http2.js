'use strict';

process.emitWarning(
  'The http2 module is an experimental API.',
  'ExperimentalWarning', undefined,
  'See https://github.com/nodejs/http2'
);

const {
  constants,
  getDefaultSettings,
  getPackedSettings,
  getUnpackedSettings,
  createServer,
  createSecureServer,
  connect
} = require('internal/http2/core');

module.exports = {
  constants,
  getDefaultSettings,
  getPackedSettings,
  getUnpackedSettings,
  createServer,
  createSecureServer,
  connect
};

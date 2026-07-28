'use strict';

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

module.exports = {
  connect,
  listen,
  DTLSEndpoint,
  DTLSSession,
};

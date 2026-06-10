'use strict';

const {
  emitExperimentalWarning,
} = require('internal/util');
emitExperimentalWarning('http3');

const {
  connect,
  listen,
  Http3Session,
  Http3Stream,
} = require('internal/quic/http3');

module.exports = {
  connect,
  listen,
  Http3Session,
  Http3Stream,
};

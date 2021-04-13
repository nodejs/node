'use strict';

const {
  emitExperimentalWarning,
} = require('internal/util');

emitExperimentalWarning('net/quic');

const {
  createEndpoint: _createEndpoint,
} = internalBinding('quic');

// If the _createEndpoint is undefined, the Node.js binary
// was built without QUIC support, in which case we
// don't want to export anything here.
if (_createEndpoint === undefined)
  return;

const {
  initializeBinding,
} = require('internal/quic/binding');

const {
  EndpointConfig,
  SessionConfig,
  StreamOptions,
  ResponseOptions,
} = require('internal/quic/config');

const {
  Http3Options,
} = require('internal/quic/http3');

const {
  Endpoint,
} = require('internal/quic/endpoint');

const {
  Session,
  ClientHello,
  OCSPRequest,
  OCSPResponse,
} = require('internal/quic/session');

const {
  Stream,
} = require('internal/quic/stream');

const {
  EndpointStats,
  SessionStats,
  StreamStats,
} = require('internal/quic/stats');

initializeBinding();

module.exports = {
  ClientHello,
  Endpoint,
  EndpointConfig,
  EndpointStats,
  Http3Options,
  OCSPRequest,
  OCSPResponse,
  ResponseOptions,
  Session,
  SessionConfig,
  SessionStats,
  Stream,
  StreamOptions,
  StreamStats,
};

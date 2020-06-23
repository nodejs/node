/* eslint-disable node-core/require-common-first, node-core/required-modules */
'use strict';

// Common bits for all QUIC-related tests
const { debuglog } = require('util');
const { readKeys } = require('./fixtures');
const { createWriteStream } = require('fs');
const kHttp3Alpn = 'h3-29';

const [ key, cert, ca ] =
  readKeys(
    'binary',
    'agent1-key.pem',
    'agent1-cert.pem',
    'ca1-cert.pem');

const debug = debuglog('test');

const kServerPort = process.env.NODE_DEBUG_KEYLOG ? 5678 : 0;
const kClientPort = process.env.NODE_DEBUG_KEYLOG ? 5679 : 0;

function setupKeylog(session) {
  if (process.env.NODE_DEBUG_KEYLOG) {
    const kl = createWriteStream(process.env.NODE_DEBUG_KEYLOG);
    session.on('keylog', kl.write.bind(kl));
  }
}

module.exports = {
  key,
  cert,
  ca,
  debug,
  kServerPort,
  kClientPort,
  setupKeylog,
  kHttp3Alpn,
};

'use strict';

const { TLSSocket, Server, createServer, connect } = require('internal/tls/wrap');
module.exports = {
  TLSSocket,
  Server,
  createServer,
  connect,
};
process.emitWarning('The _tls_wrap module is deprecated. Use `node:tls` instead.',
                    'DeprecationWarning', 'DEP0192');

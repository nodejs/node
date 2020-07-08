/* eslint-disable node-core/require-common-first, node-core/required-modules,
node-core/crypto-check */
'use strict';

const tls = require('tls');
const EventEmitter = require('events');
const DuplexPair = require('./duplexpair');

class SecurePair extends EventEmitter {
  constructor(secureContext = tls.createSecureContext(),
              isServer = false,
              requestCert = !isServer,
              rejectUnauthorized = false,
              options = {}) {
    super();
    const { clientSide: socket1, serverSide: socket2 } = new DuplexPair();

    this.server = options.server;
    this.credentials = secureContext;

    this.encrypted = socket1;
    this.cleartext = new tls.TLSSocket(socket2, {
      secureContext,
      isServer,
      requestCert,
      rejectUnauthorized,
      ...options
    });
    this.cleartext.once('secure', () => this.emit('secure'));
  }

  destroy() {
    this.cleartext.destroy();
    this.encrypted.destroy();
  }
}

module.exports = function createSecurePair(...args) {
  return new SecurePair(...args);
};

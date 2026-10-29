'use strict';

const common = require('../common');
const { addresses: { INET_HOST } } = require('../common/internet');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const { Socket } = require('net');
const { TLSSocket } = require('tls');

// Test that TLS connecting works with autoSelectFamily when using a backing socket
{
  const socket = new Socket();
  const secureSocket = new TLSSocket(socket);

  secureSocket.on('connect', common.mustCall(() => secureSocket.end()));

  socket.connect({ host: INET_HOST, port: 443, servername: INET_HOST });
  secureSocket.connect({ host: INET_HOST, port: 443, servername: INET_HOST });
}

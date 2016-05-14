'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const net = require('net');

const socket = net.connect(443, 'www.example.org', common.mustCall(() => {
  const secureSocket = tls.connect({socket: socket}, common.mustCall(() => {
    secureSocket.destroy();
    console.log('ok');
  }));
}));

'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');
const http = require('http');

// Tests that, after the HTTP parser stopped owning a socket that emits an
// 'upgrade' event, another C++ stream can start owning it (e.g. a TLSSocket).

const server = http.createServer(common.mustNotCall());

server.on('upgrade', common.mustCall((request, socket, head) => {
  // This should not crash.
  new tls.TLSSocket(socket);
  server.close();
  socket.destroy();
}));

server.listen(0, common.mustCall(() => {
  http.get({
    port: server.address().port,
    headers: {
      'Connection': 'Upgrade',
      'Upgrade': 'websocket'
    }
  }).on('error', () => {});
}));

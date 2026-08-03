// Flags: --expose-internals
'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const events = require('events');
const fixtures = require('../common/fixtures');
const tls = require('tls');
const { kReinitializeHandle } = require('internal/net');

// Test that repeated calls to kReinitializeHandle() do not result in repeatedly
// adding new listeners on the socket (i.e. no MaxListenersExceededWarnings)

process.on('warning', common.mustNotCall());

const server = tls.createServer({
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
});

server.listen(0, common.mustCall(function() {
  const socket = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  });

  socket.on('secureConnect', common.mustCall(function() {
    for (let i = 0; i < events.defaultMaxListeners + 1; i++) {
      socket[kReinitializeHandle]();
    }

    socket.destroy();
  }));

  socket.on('close', function() {
    server.close();
  });
}));

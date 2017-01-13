'use strict';
// This test binds to one port, then attempts to start a server on that
// port. It should be EADDRINUSE but be able to then bind to another port.
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server1 = net.Server();

const server2 = net.Server();

server2.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.code, 'EADDRINUSE');

  server2.listen(0, common.mustCall(function() {
    server1.close();
    server2.close();
  }));
}));

server1.listen(0, common.mustCall(function() {
  // This should make server2 emit EADDRINUSE
  server2.listen(this.address().port);
}));

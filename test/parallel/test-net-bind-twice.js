'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server1 = net.createServer(common.fail);
server1.listen(0, '127.0.0.1', common.mustCall(function() {
  const server2 = net.createServer(common.fail);
  server2.listen(this.address().port, '127.0.0.1', common.fail);

  server2.on('error', common.mustCall(function(e) {
    assert.equal(e.code, 'EADDRINUSE');
    server1.close();
  }));
}));

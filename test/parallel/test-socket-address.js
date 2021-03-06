'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

// This tests checks that if server._handle.getsockname
// returns an error number, an error is thrown.

const server = net.createServer({});
server.listen(0, common.mustCall(function() {
  server._handle.getsockname = function(out) {
    return -1;
  };
  assert.throws(() => this.address(),
                /^Error: address [\w|\s-\d]+$/);
  server.close();
}));

'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

const server1 = net.createServer(function(socket) {
});
const server2 = net.createServer(function(socket) {
});
server1.listen(0, function() {
  server2.on('error', function(error) {
    assert.equal(true, error.message.indexOf('EADDRINUSE') >= 0);
    server1.close();
  });
  server2.listen(this.address().port);
});

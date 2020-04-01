// Ensure that a net.Server without 'connection' event listeners closes
// incoming connections rather than accepting, then forgetting about them.

'use strict';
require('../common');
const net = require('net');

net.createServer(/* no handler */).listen(function() {
  const server = this;
  const { address: host, port } = server.address();
  net.connect(port, host).once('end', () => server.close());
});

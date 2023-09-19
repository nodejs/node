'use strict';
const common = require('../common');
const net = require('net');

function check(addressType) {
  const server = net.createServer(function(client) {
    client.end();
    server.close();
  });

  const address = addressType === 4 ? '127.0.0.1' : '::1';
  server.listen(0, address, function() {
    net.connect(this.address().port, address)
      .on('lookup', common.mustNotCall());
  });
}

check(4);
common.hasIPv6 && check(6);

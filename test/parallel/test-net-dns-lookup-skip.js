'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

function check(addressType) {
  var server = net.createServer(function(client) {
    client.end();
    server.close();
  });

  var address = addressType === 4 ? '127.0.0.1' : '::1';
  server.listen(common.PORT, address, function() {
    net.connect(common.PORT, address).on('lookup', common.fail);
  });
}

check(4);
common.hasIPv6 && check(6);

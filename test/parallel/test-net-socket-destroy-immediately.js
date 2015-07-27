'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var socket = net.createConnection(common.PORT, common.localhostIPv4);

socket.on('error', function(err) {
  assert(false);
});

socket.destroy();

'use strict';
const common = require('../common');
var net = require('net');

var client;
var TIMEOUT = 10 * 1000;

client = net.createConnection(53, '8.8.8.8', function() {
  client.unref();
});

client.on('close', common.fail);

setTimeout(common.fail, TIMEOUT).unref();

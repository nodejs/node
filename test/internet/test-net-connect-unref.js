'use strict';
const common = require('../common');
const net = require('net');

const TIMEOUT = 10 * 1000;

const client = net.createConnection(53, '8.8.8.8', function() {
  client.unref();
});

client.on('close', common.mustNotCall());

setTimeout(common.mustNotCall(), TIMEOUT).unref();

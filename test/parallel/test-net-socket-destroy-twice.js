'use strict';
const common = require('../common');
const net = require('net');

const conn = net.createConnection(common.PORT);

conn.on('error', common.mustCall(function() {
  conn.destroy();
}));

conn.on('close', common.mustCall(function() {}));

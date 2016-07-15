'use strict';
var common = require('../common');
var net = require('net');

var conn = net.createConnection(common.PORT);

conn.on('error', common.mustCall(function() {
  conn.destroy();
}));

conn.on('close', common.mustCall(function() {}));

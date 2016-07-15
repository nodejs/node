'use strict';
const common = require('../common');
var net = require('net');

var server = net.createServer(function(socket) {
});
server.listen(1, '1.1.1.1', common.fail); // EACCESS or EADDRNOTAVAIL
server.on('error', common.mustCall(function(error) {}));

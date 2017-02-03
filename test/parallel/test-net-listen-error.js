'use strict';
const common = require('../common');
const net = require('net');

const server = net.createServer(function(socket) {
});
server.listen(1, '1.1.1.1', common.mustNotCall()); // EACCESS or EADDRNOTAVAIL
server.on('error', common.mustCall(function(error) {}));
